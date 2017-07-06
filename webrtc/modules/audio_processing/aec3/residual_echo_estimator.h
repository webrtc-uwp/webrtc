/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_RESIDUAL_ECHO_ESTIMATOR_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_RESIDUAL_ECHO_ESTIMATOR_H_

#include <algorithm>
#include <array>
#include <vector>

#include "webrtc/modules/audio_processing/aec3/aec3_common.h"
#include "webrtc/modules/audio_processing/aec3/aec_state.h"
#include "webrtc/modules/audio_processing/aec3/render_buffer.h"
#include "webrtc/rtc_base/array_view.h"
#include "webrtc/rtc_base/constructormagic.h"

namespace webrtc {

class ResidualEchoEstimator {
 public:
  ResidualEchoEstimator();
  ~ResidualEchoEstimator();

  void Estimate(bool using_subtractor_output,
                const AecState& aec_state,
                const RenderBuffer& render_buffer,
                const std::array<float, kFftLengthBy2Plus1>& S2_linear,
                const std::array<float, kFftLengthBy2Plus1>& Y2,
                std::array<float, kFftLengthBy2Plus1>* R2);

 private:
  // Resets the state.
  void Reset();

  // Estimates the residual echo power based on the echo return loss enhancement
  // (ERLE) and the linear power estimate.
  void LinearEstimate(const std::array<float, kFftLengthBy2Plus1>& S2_linear,
                      const std::array<float, kFftLengthBy2Plus1>& erle,
                      size_t delay,
                      std::array<float, kFftLengthBy2Plus1>* R2);

  // Estimates the residual echo power based on the estimate of the echo path
  // gain.
  void NonLinearEstimate(float echo_path_gain,
                         const std::array<float, kFftLengthBy2Plus1>& X2,
                         const std::array<float, kFftLengthBy2Plus1>& Y2,
                         std::array<float, kFftLengthBy2Plus1>* R2);

  // Adds the estimated unmodelled echo power to the residual echo power
  // estimate.
  void AddEchoReverb(const std::array<float, kFftLengthBy2Plus1>& S2,
                     bool saturated_echo,
                     size_t delay,
                     float reverb_decay_factor,
                     std::array<float, kFftLengthBy2Plus1>* R2);

  std::array<float, kFftLengthBy2Plus1> R2_old_;
  std::array<int, kFftLengthBy2Plus1> R2_hold_counter_;
  std::array<float, kFftLengthBy2Plus1> R2_reverb_;
  int S2_old_index_ = 0;
  std::array<std::array<float, kFftLengthBy2Plus1>, kAdaptiveFilterLength>
      S2_old_;
  std::array<float, kFftLengthBy2Plus1> X2_noise_floor_;
  std::array<int, kFftLengthBy2Plus1> X2_noise_floor_counter_;

  RTC_DISALLOW_COPY_AND_ASSIGN(ResidualEchoEstimator);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_RESIDUAL_ECHO_ESTIMATOR_H_
