/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/agc2/gain_controller2.h"

#include <cmath>

#include "webrtc/modules/audio_processing/audio_buffer.h"
#include "webrtc/modules/audio_processing/logging/apm_data_dumper.h"
#include "webrtc/rtc_base/atomicops.h"
#include "webrtc/rtc_base/checks.h"

namespace webrtc {

int GainController2::instance_count_ = 0;

GainController2::GainController2(const float fixed_gain_db)
    : data_dumper_(new ApmDataDumper(instance_count_)),
      fixed_gain_(std::pow(10.0, fixed_gain_db / 20.0)) {
  Initialize(AudioProcessing::kSampleRate48kHz);
  ++instance_count_;
}

GainController2::~GainController2() = default;

void GainController2::Initialize(int sample_rate_hz) {
  RTC_DCHECK(sample_rate_hz == AudioProcessing::kSampleRate8kHz ||
             sample_rate_hz == AudioProcessing::kSampleRate16kHz ||
             sample_rate_hz == AudioProcessing::kSampleRate32kHz ||
             sample_rate_hz == AudioProcessing::kSampleRate48kHz);
  data_dumper_->InitiateNewSetOfRecordings();
  data_dumper_->DumpRaw("fixed gain (linear)", fixed_gain_);
  sample_rate_hz_ = sample_rate_hz;
}

void GainController2::Process(AudioBuffer* audio) {
  bool saturated_frame = false;
  for (size_t k = 0; k < audio->num_channels(); ++k) {
    for (size_t j = 0; j < audio->num_frames(); ++j) {
      audio->channels_f()[k][j] = std::min(
          32767.f, std::max(-32768.f, fixed_gain_ * audio->channels_f()[k][j]));
      if (audio->channels_f()[k][j] == -32768.f ||
          audio->channels_f()[k][j] == 32767.f) {
        saturated_frame = true;
      }
    }
  }

  if (saturated_frame) {
    data_dumper_->DumpRaw("saturated frame detected", true);
  }
}

bool GainController2::Validate(
    const AudioProcessing::Config::GainController2& config) {
  return config.fixed_gain_db >= 0.f;
}

std::string GainController2::ToString(
    const AudioProcessing::Config::GainController2& config) {
  std::stringstream ss;
  ss << "{"
     << "enabled: " << (config.enabled ? "true" : "false") << ", "
     << "fixed_gain_dB: " << config.fixed_gain_db << "}";
  return ss.str();
}

}  // namespace webrtc
