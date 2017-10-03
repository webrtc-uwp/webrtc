/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef EXAMPLES_UNITYPLUGIN_VRAUDIO_VRAUDIO_WRAP_H_
#define EXAMPLES_UNITYPLUGIN_VRAUDIO_VRAUDIO_WRAP_H_

#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include "api/audio/audio_mixer.h"
#include "modules/audio_coding/neteq/audio_vector.h"

namespace vraudio {
class VrAudioApi;
}

class VrAudioWrap : public webrtc::AudioMixer {
 public:
  // Constructor and destructor;
  explicit VrAudioWrap(bool zero_padding);
  ~VrAudioWrap() override;

  // Override for webrtc::Audiomixer.
  // Returns true if adding was successful. A source is never added
  // twice. Addition and removal can happen on different threads.
  bool AddSource(Source* audio_source) override;

  // Remove a source.
  void RemoveSource(Source* audio_source) override;

  // Performs mixing by asking registered audio sources for audio. The
  // mixed result is placed in the provided AudioFrame. This method
  // will only be called from a single thread. The channels argument
  // specifies the number of channels of the mix result. The mixer
  // should mix at a rate that doesn't cause quality loss of the
  // sources' audio. The mixing rate is one of the rates listed in
  // AudioProcessing::NativeRate. All fields in
  // |audio_frame_for_mixing| must be updated.
  void Mix(size_t number_of_channels,
           webrtc::AudioFrame* audio_frame_for_mixing) override;

  // vraudio_api functions
  // Set listener head position/rotation.
  bool SetListenerHeadPosition(float x, float y, float z);
  bool SetListenerHeadRotation(float x, float y, float z, float w);
  // Set audio source position/rotation.
  bool SetAudioSourcePosition(uint32_t ssrc, float x, float y, float z);
  bool SetAudioSourceRotation(uint32_t ssrc,
                              float x,
                              float y,
                              float z,
                              float w);

  // Return true if audio spatializer is initialized.
  bool IsInitialized();

 private:
  struct Pose {
    // Position.
    float x = 0;
    float y = 0;
    float z = 0;
    // Quaternion Rotation.
    float rx = 0;
    float ry = 0;
    float rz = 0;
    float rw = 1;
  };

  struct AudioSourrceInfo {
    // Audio source id for spatializer.
    int source_id = -1;
    // Audio source defined by webrtc.
    Source* audio_source = nullptr;
    // Data frame of audio source.
    webrtc::AudioFrame audio_frame;
    // True if data in audio_frame is valid and not processed.
    bool has_data_to_process = false;
    // Number of channels of audio source.
    size_t num_channels = 1;
    // Input ring buffer and prcoessing buffere for spatialization.
    std::unique_ptr<int16_t[]> process_buf;
    std::unique_ptr<webrtc::AudioVector> ring_buf;
    // Audio source's pose.
    Pose pose;
  };

  void Initialize(int sample_rate_hz);
  int CalculateMixingFrequency();
  void SetupForAudioSource(AudioSourrceInfo* info);
  bool Mix_zeroPadding(webrtc::AudioFrame* audio_frame_for_mixing);
  bool Mix_ringBuffers(webrtc::AudioFrame* audio_frame_for_mixing);

  vraudio::VrAudioApi* spatializer_ = nullptr;
  bool zero_padding_ = false;
  int sample_rate_hz_ = -1;
  // The samples per vraudio frame/processing buffer.
  size_t samples_per_buf_ = 0;
  // The samples per webrtc frame.
  size_t samples_per_frame_ = 0;
  // The time_stamp for webrtc frame.
  uint32_t time_stamp_ = 0;

  std::map<uint32_t, AudioSourrceInfo> source_infos_;

  // Mutex for audio sources.
  std::recursive_mutex source_mutex_;
  // Mutex for spatializer.
  std::recursive_mutex spatializer_mutex_;

  // Number of audio samples in input ring buffers.
  int samples_in_input_ring_buf_ = 0;

  // Position and rotation of listener's head.
  Pose head_pose_;

  // Output ring buffer and processing buffer for spatialization.
  std::unique_ptr<int16_t[]> out_process_buf_;
  std::unique_ptr<webrtc::AudioVector> out_ring_buf_;
};

#endif  // EXAMPLES_UNITYPLUGIN_VRAUDIO_VRAUDIO_WRAP_H_
