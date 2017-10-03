/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "examples/unityplugin/vraudio/vraudio_wrap.h"

#include <algorithm>

#include "audio/utility/audio_frame_operations.h"
#include "examples/unityplugin/vraudio/vraudio_api.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "rtc_base/logging.h"

static const int kDefaultFrequency = 48000;

VrAudioWrap::VrAudioWrap(bool zero_padding) {
  zero_padding_ = zero_padding;
}

VrAudioWrap::~VrAudioWrap() {
  if (spatializer_)
    vraudio::VrAudioApi::Destroy(&spatializer_);
}

bool VrAudioWrap::AddSource(Source* audio_source) {
  // Sanity check.
  if (audio_source == nullptr)
    return false;

  std::lock_guard<std::recursive_mutex> lock(source_mutex_);

  // The source already exists.
  uint32_t ssrc = audio_source->Ssrc();
  if (source_infos_.count(ssrc))
    return false;

  source_infos_[ssrc].audio_source = audio_source;

  {
    // Add to spatializer_.
    std::lock_guard<std::recursive_mutex> lock(spatializer_mutex_);
    if (spatializer_)
      SetupForAudioSource(&source_infos_[ssrc]);
  }
  return true;
}

void VrAudioWrap::RemoveSource(Source* audio_source) {
  // Sanity check.
  if (audio_source == nullptr)
    return;

  std::lock_guard<std::recursive_mutex> lock(source_mutex_);

  // The source doesn't exists.
  uint32_t ssrc = audio_source->Ssrc();
  if (source_infos_.count(ssrc) != 1)
    return;

  {
    // Remove from spatializer_.
    std::lock_guard<std::recursive_mutex> lock(spatializer_mutex_);
    if (spatializer_)
      spatializer_->DestroySource(source_infos_[ssrc].source_id);
  }

  source_infos_.erase(ssrc);
  return;
}

void VrAudioWrap::Mix(size_t number_of_channels,
                      webrtc::AudioFrame* audio_frame_for_mixing) {
  std::lock_guard<std::recursive_mutex> lock(source_mutex_);

  // Initialize/re initialize if the sample rate has changed.
  int sample_rate_hz = CalculateMixingFrequency();
  if (sample_rate_hz_ != sample_rate_hz)
    Initialize(sample_rate_hz);

  // Setup the output frame.
  audio_frame_for_mixing->UpdateFrame(
      time_stamp_, NULL, samples_per_frame_, sample_rate_hz_,
      webrtc::AudioFrame::kNormalSpeech, webrtc::AudioFrame::kVadPassive, 2);
  time_stamp_ += static_cast<uint32_t>(samples_per_frame_);

  // Get input audio frames from sources.
  for (auto& item : source_infos_) {
    AudioSourrceInfo& info = item.second;
    const auto status = info.audio_source->GetAudioFrameWithInfo(
        sample_rate_hz_, &info.audio_frame);
    if (status == webrtc::AudioMixer::Source::AudioFrameInfo::kNormal)
      info.has_data_to_process = true;
    else
      info.has_data_to_process = false;

    if (status != webrtc::AudioMixer::Source::AudioFrameInfo::kError)
      info.num_channels = info.audio_frame.num_channels_;
  }

  // Do spatialization mixing.
  if (zero_padding_)
    Mix_zeroPadding(audio_frame_for_mixing);
  else
    Mix_ringBuffers(audio_frame_for_mixing);

  // Convert stereo to mono if the output need to be mono.
  if (number_of_channels == 1 && audio_frame_for_mixing->num_channels_ == 2)
    webrtc::AudioFrameOperations::StereoToMono(audio_frame_for_mixing);
}

bool VrAudioWrap::IsInitialized() {
  std::lock_guard<std::recursive_mutex> lock(spatializer_mutex_);
  return spatializer_ == nullptr;
}

// vraudio_api functions.
bool VrAudioWrap::SetListenerHeadPosition(float x, float y, float z) {
  std::lock_guard<std::recursive_mutex> lock(spatializer_mutex_);

  if (spatializer_ == nullptr)
    return false;

  spatializer_->SetHeadPosition(x, y, z);
  head_pose_.x = x;
  head_pose_.y = y;
  head_pose_.z = z;

  return true;
}

bool VrAudioWrap::SetListenerHeadRotation(float x, float y, float z, float w) {
  std::lock_guard<std::recursive_mutex> lock(spatializer_mutex_);

  if (spatializer_ == nullptr)
    return false;

  spatializer_->SetHeadRotation(x, y, z, w);
  head_pose_.rx = x;
  head_pose_.ry = y;
  head_pose_.rz = z;
  head_pose_.rw = w;

  return true;
}

bool VrAudioWrap::SetAudioSourcePosition(uint32_t ssrc,
                                         float x,
                                         float y,
                                         float z) {
  std::lock_guard<std::recursive_mutex> lock(spatializer_mutex_);

  if (spatializer_ == nullptr)
    return false;

  int audio_id;
  {
    std::lock_guard<std::recursive_mutex> lock(source_mutex_);
    if (!source_infos_.count(ssrc))
      return false;

    audio_id = source_infos_[ssrc].source_id;
    source_infos_[ssrc].pose.x = x;
    source_infos_[ssrc].pose.y = y;
    source_infos_[ssrc].pose.z = z;
  }
  if (audio_id < 0)
    return false;

  spatializer_->SetSourcePosition(audio_id, x, y, z);

  return true;
}

bool VrAudioWrap::SetAudioSourceRotation(uint32_t ssrc,
                                         float x,
                                         float y,
                                         float z,
                                         float w) {
  std::lock_guard<std::recursive_mutex> lock(spatializer_mutex_);

  if (spatializer_ == nullptr)
    return false;

  int audio_id;
  {
    std::lock_guard<std::recursive_mutex> lock(source_mutex_);
    if (!source_infos_.count(ssrc))
      return false;

    audio_id = source_infos_[ssrc].source_id;
    source_infos_[ssrc].pose.rx = x;
    source_infos_[ssrc].pose.ry = y;
    source_infos_[ssrc].pose.rz = z;
    source_infos_[ssrc].pose.rw = w;
  }
  if (audio_id < 0)
    return false;

  spatializer_->SetSourceRotation(audio_id, x, y, z, w);
  return true;
}

void VrAudioWrap::Initialize(int sample_rate_hz) {
  sample_rate_hz_ = sample_rate_hz;
  // Each webrtc frame alway contains 10ms of audio data.
  samples_per_frame_ = sample_rate_hz / 100;

  // Samples for vraudio's processing buffer.
  samples_per_buf_ = samples_per_frame_;

  // Find max samples_per_buf that is power of 2 and smaller or equal to
  // samples_per_frame_.
  if (!zero_padding_) {
    samples_per_buf_ = 64;
    while (samples_per_buf_ * 2 <= samples_per_frame_)
      samples_per_buf_ *= 2;
  }

  {
    std::lock_guard<std::recursive_mutex> lock(spatializer_mutex_);

    // Destroy exist spatializer if necessary.
    if (spatializer_)
      vraudio::VrAudioApi::Destroy(&spatializer_);

    spatializer_ =
        vraudio::VrAudioApi::Create(2, samples_per_buf_, sample_rate_hz_);
  }

  // Set listener's head position and rotation.
  spatializer_->SetHeadPosition(head_pose_.x, head_pose_.y, head_pose_.z);
  spatializer_->SetHeadRotation(head_pose_.rx, head_pose_.ry, head_pose_.rz,
                                head_pose_.rw);

  // Create mixed/output audio related buffers.
  if (!zero_padding_) {
    out_process_buf_.reset(new int16_t[samples_per_buf_ * 2]);
    out_ring_buf_.reset(new webrtc::AudioVector());
    samples_in_input_ring_buf_ = 0;
  }

  // Set for each audio source.
  for (auto& item : source_infos_) {
    AudioSourrceInfo& info = item.second;
    SetupForAudioSource(&info);
  }
}

// Rounds the maximal audio source frequency up to an APM-native frequency.
int VrAudioWrap::CalculateMixingFrequency() {
  if (source_infos_.empty())
    return kDefaultFrequency;

  using NativeRate = webrtc::AudioProcessing::NativeRate;
  int maximal_frequency = 0;
  for (auto& item : source_infos_) {
    const int source_needed_frequency =
        item.second.audio_source->PreferredSampleRate();
    maximal_frequency = std::max(maximal_frequency, source_needed_frequency);
  }

  if (maximal_frequency < NativeRate::kSampleRate8kHz ||
      maximal_frequency > NativeRate::kSampleRate48kHz)
    return kDefaultFrequency;

  static constexpr NativeRate native_rates[] = {
      NativeRate::kSampleRate8kHz, NativeRate::kSampleRate16kHz,
      NativeRate::kSampleRate32kHz, NativeRate::kSampleRate48kHz};
  const auto rounded_up_index = std::lower_bound(
      std::begin(native_rates), std::end(native_rates), maximal_frequency);
  if (rounded_up_index == std::end(native_rates))
    return kDefaultFrequency;

  return *rounded_up_index;
}

void VrAudioWrap::SetupForAudioSource(AudioSourrceInfo* info) {
  if (spatializer_ == nullptr)
    return;
  int source_id = spatializer_->CreateSoundObjectSource(
      vraudio::VrAudioApi::kBinauralLowQuality);
  info->source_id = source_id;

  Pose pose = info->pose;
  spatializer_->SetSourcePosition(source_id, pose.x, pose.y, pose.z);
  spatializer_->SetSourceRotation(source_id, pose.rx, pose.ry, pose.rz,
                                  pose.rw);

  if (!zero_padding_) {
    info->process_buf.reset(new int16_t[samples_per_buf_ * 2]);
    info->ring_buf.reset(new webrtc::AudioVector());
  }
}

bool VrAudioWrap::Mix_zeroPadding(webrtc::AudioFrame* audio_frame_for_mixing) {
  int source_count = 0;

  // Fill input audio data for each source.
  for (auto& item : source_infos_) {
    AudioSourrceInfo& info = item.second;
    if (!info.has_data_to_process)
      continue;

    source_count++;
    const webrtc::AudioFrame* frame = &(info.audio_frame);
    spatializer_->SetInterleavedBuffer(info.source_id, frame->data(),
                                       frame->num_channels_, samples_per_buf_);
  }

  if (source_count > 0) {
    // If there are inputs.
    spatializer_->FillInterleavedOutputBuffer(
        2, samples_per_buf_, audio_frame_for_mixing->mutable_data());
  } else {
    // Mute if there is no input.
    audio_frame_for_mixing->Mute();
  }

  return true;
}

bool VrAudioWrap::Mix_ringBuffers(webrtc::AudioFrame* audio_frame_for_mixing) {
  // Update ring buffer for each audio source.
  for (auto& item : source_infos_) {
    AudioSourrceInfo& info = item.second;

    // Adjust the ring buffer content size to take care of new joined.
    // participants and participants with the changed number of channels.
    webrtc::AudioVector* ring_buf = info.ring_buf.get();
    const size_t buf_size_to_be =
        samples_in_input_ring_buf_ * info.num_channels;
    const size_t buf_size = ring_buf->Size();
    if (buf_size > buf_size_to_be) {
      ring_buf->PopFront(buf_size - buf_size_to_be);
    } else if (buf_size < buf_size_to_be) {
      // The extended part is set to zeros.
      ring_buf->Extend(buf_size_to_be - buf_size);
    }

    // Fill ring bufs with frame or zeros.
    if (info.has_data_to_process) {
      const webrtc::AudioFrame* frame = &(info.audio_frame);
      ring_buf->PushBack(frame->data(),
                         frame->samples_per_channel_ * frame->num_channels_);
    } else {
      // Add zeros to the back.
      ring_buf->Extend(info.num_channels * samples_per_frame_);
    }
  }
  samples_in_input_ring_buf_ += static_cast<int>(samples_per_frame_);

  // Spatialization process for samples in input ring buffers until there is
  // not enough data available for spatializer.
  while (samples_in_input_ring_buf_ >= static_cast<int>(samples_per_buf_)) {
    for (auto& item : source_infos_) {
      AudioSourrceInfo& info = item.second;
      // Fill input data to spatializer.
      webrtc::AudioVector* ring_buf = info.ring_buf.get();
      int16_t* in_buf = info.process_buf.get();
      ring_buf->CopyTo(info.num_channels * samples_per_buf_, 0, in_buf);
      ring_buf->PopFront(info.num_channels * samples_per_buf_);
      spatializer_->SetInterleavedBuffer(info.source_id, in_buf,
                                         info.num_channels, samples_per_buf_);
    }

    // Get spatialized audio and fill into output ring buffer.
    int16_t* outBuf = out_process_buf_.get();
    spatializer_->FillInterleavedOutputBuffer(2, samples_per_buf_, outBuf);
    out_ring_buf_->PushBack(outBuf, samples_per_buf_ * 2);

    samples_in_input_ring_buf_ -= static_cast<int>(samples_per_buf_);
  }

  // Fill the mixed frame.
  const int contentSize = static_cast<int>(out_ring_buf_->Size());
  if (contentSize < static_cast<int>(samples_per_frame_) * 2) {
    // Partially fill the mixed frame.
    out_ring_buf_->CopyTo(contentSize, 0,
                          audio_frame_for_mixing->mutable_data());
    out_ring_buf_->Clear();
    // Fill zeros for remaining mixed frame.
    memset(audio_frame_for_mixing->mutable_data() + contentSize, 0,
           (samples_per_frame_ * 2 - contentSize) * sizeof(int16_t));
  } else {
    out_ring_buf_->CopyTo(samples_per_frame_ * 2, 0,
                          audio_frame_for_mixing->mutable_data());
    out_ring_buf_->PopFront(samples_per_frame_ * 2);
  }

  return true;
}
