/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <string>

#include "webrtc/modules/audio_processing/agc2/gain_controller2.h"
#include "webrtc/modules/audio_processing/audio_buffer.h"
#include "webrtc/rtc_base/array_view.h"
#include "webrtc/test/gtest.h"

namespace webrtc {
namespace test {

namespace {

constexpr size_t kNumFrames = 480u;
constexpr size_t kStereo = 2u;

void SetAudioBufferSamples(float value, AudioBuffer* ab) {
  for (size_t k = 0; k < ab->num_channels(); ++k) {
    auto channel = rtc::ArrayView<float>(ab->channels_f()[k], ab->num_frames());
    for (auto& sample : channel) { sample = value; }
  }
}

}  // namespace

TEST(GainController2, Instance) {
  std::unique_ptr<GainController2> gain_controller2;
  gain_controller2.reset(new GainController2(5.f));
}

TEST(GainController2, ToString) {
  AudioProcessing::Config config;
  config.gain_controller2.fixed_gain_db = 5.f;

  config.gain_controller2.enabled = false;
  EXPECT_EQ("{enabled: false, fixed_gain_dB: 5.0}",
            GainController2::ToString(config.gain_controller2));

  config.gain_controller2.enabled = true;
  EXPECT_EQ("{enabled: true, fixed_gain_dB: 5.0}",
            GainController2::ToString(config.gain_controller2));
}

TEST(GainController2, Usage) {
  std::unique_ptr<GainController2> gain_controller2;
  gain_controller2.reset(new GainController2(5.f));
  AudioBuffer ab(kNumFrames, kStereo, kNumFrames, kStereo, kNumFrames);
  SetAudioBufferSamples(1000.0f, &ab);
  gain_controller2->Process(&ab);
}

}  // namespace test
}  // namespace webrtc
