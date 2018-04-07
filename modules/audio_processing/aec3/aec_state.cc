/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/aec_state.h"

#include <math.h>

#include <numeric>
#include <vector>

#include "api/array_view.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/atomicops.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace {

float ComputeGainRampupIncrease(const EchoCanceller3Config& config) {
  const auto& c = config.echo_removal_control.gain_rampup;
  return powf(1.f / c.first_non_zero_gain, 1.f / c.non_zero_gain_blocks);
}

constexpr size_t kBlocksSinceConvergencedFilterInit = 10000;
constexpr size_t kBlocksSinceConsistentEstimateInit = 10000;

}  // namespace

int AecState::instance_count_ = 0;

AecState::AecState(const EchoCanceller3Config& config)
    : data_dumper_(
          new ApmDataDumper(rtc::AtomicOps::Increment(&instance_count_))),
      erle_estimator_(config.erle.min, config.erle.max_l, config.erle.max_h),
      config_(config),
      max_render_(config_.filter.main.length_blocks, 0.f),
      reverb_decay_(fabsf(config_.ep_strength.default_len)),
      gain_rampup_increase_(ComputeGainRampupIncrease(config_)),
      suppression_gain_limiter_(config_),
      filter_analyzer_(config_),
      blocks_since_converged_filter_(kBlocksSinceConvergencedFilterInit),
      active_blocks_since_consistent_filter_estimate_(
          kBlocksSinceConsistentEstimateInit) {}

AecState::~AecState() = default;

void AecState::HandleEchoPathChange(
    const EchoPathVariability& echo_path_variability) {
  const auto full_reset = [&]() {
    filter_analyzer_.Reset();
    blocks_since_last_saturation_ = 0;
    usable_linear_estimate_ = false;
    capture_signal_saturation_ = false;
    echo_saturation_ = false;
    previous_max_sample_ = 0.f;
    std::fill(max_render_.begin(), max_render_.end(), 0.f);
    blocks_with_proper_filter_adaptation_ = 0;
    blocks_since_reset_ = 0;
    filter_has_had_time_to_converge_ = false;
    render_received_ = false;
    blocks_with_active_render_ = 0;
    initial_state_ = true;
    suppression_gain_limiter_.Reset();
    blocks_since_converged_filter_ = kBlocksSinceConvergencedFilterInit;
    diverged_blocks_ = 0;
  };

  // TODO(peah): Refine the reset scheme according to the type of gain and
  // delay adjustment.
  if (echo_path_variability.gain_change) {
    full_reset();
  }

  if (echo_path_variability.delay_change !=
      EchoPathVariability::DelayAdjustment::kBufferReadjustment) {
    full_reset();
  } else if (echo_path_variability.delay_change !=
             EchoPathVariability::DelayAdjustment::kBufferFlush) {
    full_reset();
  } else if (echo_path_variability.delay_change !=
             EchoPathVariability::DelayAdjustment::kDelayReset) {
    full_reset();
  } else if (echo_path_variability.delay_change !=
             EchoPathVariability::DelayAdjustment::kNewDetectedDelay) {
    full_reset();
  } else if (echo_path_variability.gain_change) {
    blocks_since_reset_ = kNumBlocksPerSecond;
  }
}

void AecState::Update(
    const rtc::Optional<DelayEstimate>& external_delay,
    const std::vector<std::array<float, kFftLengthBy2Plus1>>&
        adaptive_filter_frequency_response,
    const std::vector<float>& adaptive_filter_impulse_response,
    bool converged_filter,
    bool diverged_filter,
    const RenderBuffer& render_buffer,
    const std::array<float, kFftLengthBy2Plus1>& E2_main,
    const std::array<float, kFftLengthBy2Plus1>& Y2,
    const std::array<float, kBlockSize>& s) {
  // Analyze the filter and compute the delays.
  filter_analyzer_.Update(adaptive_filter_impulse_response, render_buffer);
  filter_delay_blocks_ = filter_analyzer_.DelayBlocks();

  if (filter_analyzer_.Consistent()) {
    internal_delay_ = filter_analyzer_.DelayBlocks();
  } else {
    internal_delay_ = rtc::nullopt;
  }

  external_delay_seen_ = external_delay_seen_ || external_delay;

  const std::vector<float>& x = render_buffer.Block(-filter_delay_blocks_)[0];

  // Update counters.
  ++capture_block_counter_;
  ++blocks_since_reset_;
  const bool active_render_block = DetectActiveRender(x);
  blocks_with_active_render_ += active_render_block ? 1 : 0;
  blocks_with_proper_filter_adaptation_ +=
      active_render_block && !SaturatedCapture() ? 1 : 0;

  // Update the limit on the echo suppression after an echo path change to avoid
  // an initial echo burst.
  suppression_gain_limiter_.Update(render_buffer.GetRenderActivity(),
                                   transparent_mode_);

  // Update the ERL and ERLE measures.
  if (converged_filter && blocks_since_reset_ >= 2 * kNumBlocksPerSecond) {
    const auto& X2 = render_buffer.Spectrum(filter_delay_blocks_);
    erle_estimator_.Update(X2, Y2, E2_main);
    erl_estimator_.Update(X2, Y2);
  }

  // Detect and flag echo saturation.
  // TODO(peah): Add the delay in this computation to ensure that the render and
  // capture signals are properly aligned.
  if (config_.ep_strength.echo_can_saturate) {
    echo_saturation_ = DetectEchoSaturation(x);
  }

  bool filter_has_had_time_to_converge =
      blocks_with_proper_filter_adaptation_ >= 1.5f * kNumBlocksPerSecond;

  if (!filter_should_have_converged_) {
    filter_should_have_converged_ =
        blocks_with_proper_filter_adaptation_ > 6 * kNumBlocksPerSecond;
  }

  // Flag whether the initial state is still active.
  initial_state_ =
      blocks_with_proper_filter_adaptation_ < 5 * kNumBlocksPerSecond;

  // Update counters for the filter divergence and convergence.
  diverged_blocks_ = diverged_filter ? diverged_blocks_ + 1 : 0;
  if (diverged_blocks_ >= 60) {
    blocks_since_converged_filter_ = kBlocksSinceConvergencedFilterInit;
  } else {
    blocks_since_converged_filter_ =
        converged_filter ? 0 : blocks_since_converged_filter_ + 1;
  }
  if (converged_filter) {
    active_blocks_since_converged_filter_ = 0;
  } else if (active_render_block) {
    ++active_blocks_since_converged_filter_;
  }

  bool recently_converged_filter =
      blocks_since_converged_filter_ < 60 * kNumBlocksPerSecond;

  if (blocks_since_converged_filter_ > 20 * kNumBlocksPerSecond) {
    converged_filter_count_ = 0;
  } else if (converged_filter) {
    ++converged_filter_count_;
  }
  if (converged_filter_count_ > 50) {
    finite_erl_ = true;
  }

  if (filter_analyzer_.Consistent() && filter_delay_blocks_ < 5) {
    consistent_filter_seen_ = true;
    active_blocks_since_consistent_filter_estimate_ = 0;
  } else if (active_render_block) {
    ++active_blocks_since_consistent_filter_estimate_;
  }

  bool consistent_filter_estimate_not_seen;
  if (!consistent_filter_seen_) {
    consistent_filter_estimate_not_seen =
        capture_block_counter_ > 5 * kNumBlocksPerSecond;
  } else {
    consistent_filter_estimate_not_seen =
        active_blocks_since_consistent_filter_estimate_ >
        30 * kNumBlocksPerSecond;
  }

  converged_filter_seen_ = converged_filter_seen_ || converged_filter;

  // If no filter convergence is seen for a long time, reset the estimated
  // properties of the echo path.
  if (active_blocks_since_converged_filter_ > 60 * kNumBlocksPerSecond) {
    converged_filter_seen_ = false;
    finite_erl_ = false;
  }

  // After an amount of active render samples for which an echo should have been
  // detected in the capture signal if the ERL was not infinite, flag that a
  // transparent mode should be entered.
  transparent_mode_ = !config_.ep_strength.bounded_erl && !finite_erl_;
  transparent_mode_ =
      transparent_mode_ &&
      (consistent_filter_estimate_not_seen || !converged_filter_seen_);
  transparent_mode_ = transparent_mode_ &&
                      (filter_should_have_converged_ ||
                       (!external_delay_seen_ &&
                        capture_block_counter_ > 10 * kNumBlocksPerSecond));

  usable_linear_estimate_ = !echo_saturation_;
  usable_linear_estimate_ =
      usable_linear_estimate_ && filter_has_had_time_to_converge;
  usable_linear_estimate_ =
      usable_linear_estimate_ && recently_converged_filter;
  usable_linear_estimate_ = usable_linear_estimate_ && !diverged_filter;
  usable_linear_estimate_ = usable_linear_estimate_ && external_delay;

  use_linear_filter_output_ = usable_linear_estimate_ && !TransparentMode();

  data_dumper_->DumpRaw("aec3_erle", Erle());
  data_dumper_->DumpRaw("aec3_erl", Erl());
  data_dumper_->DumpRaw("aec3_erle_time_domain", ErleTimeDomain());
  data_dumper_->DumpRaw("aec3_erl_time_domain", ErlTimeDomain());
  data_dumper_->DumpRaw("aec3_usable_linear_estimate", UsableLinearEstimate());
  data_dumper_->DumpRaw("aec3_transparent_mode", transparent_mode_);
  data_dumper_->DumpRaw("aec3_state_internal_delay",
                        internal_delay_ ? *internal_delay_ : -1);
  data_dumper_->DumpRaw("aec3_filter_delay", filter_analyzer_.DelayBlocks());

  data_dumper_->DumpRaw("aec3_consistent_filter",
                        filter_analyzer_.Consistent());
  data_dumper_->DumpRaw("aec3_suppression_gain_limit", SuppressionGainLimit());
  data_dumper_->DumpRaw("aec3_initial_state", InitialState());
  data_dumper_->DumpRaw("aec3_capture_saturation", SaturatedCapture());
  data_dumper_->DumpRaw("aec3_echo_saturation", echo_saturation_);
  data_dumper_->DumpRaw("aec3_converged_filter", converged_filter);
  data_dumper_->DumpRaw("aec3_diverged_filter", diverged_filter);

  data_dumper_->DumpRaw("aec3_external_delay_avaliable",
                        external_delay ? 1 : 0);
  data_dumper_->DumpRaw("aec3_consistent_filter_estimate_not_seen",
                        consistent_filter_estimate_not_seen);
  data_dumper_->DumpRaw("aec3_filter_should_have_converged",
                        filter_should_have_converged_);
  data_dumper_->DumpRaw("aec3_filter_has_had_time_to_converge",
                        filter_has_had_time_to_converge);
  data_dumper_->DumpRaw("aec3_recently_converged_filter",
                        recently_converged_filter);
}

void AecState::UpdateReverb(const std::vector<float>& impulse_response) {
  if ((!(filter_delay_blocks_ && usable_linear_estimate_)) ||
      (filter_delay_blocks_ >
       static_cast<int>(config_.filter.main.length_blocks) - 4)) {
    return;
  }

  // Form the data to match against by squaring the impulse response
  // coefficients.
  std::array<float, GetTimeDomainLength(kMaxAdaptiveFilterLength)>
      matching_data_data;
  RTC_DCHECK_LE(GetTimeDomainLength(config_.filter.main.length_blocks),
                matching_data_data.size());
  rtc::ArrayView<float> matching_data(
      matching_data_data.data(),
      GetTimeDomainLength(config_.filter.main.length_blocks));
  std::transform(impulse_response.begin(), impulse_response.end(),
                 matching_data.begin(), [](float a) { return a * a; });

  // Avoid matching against noise in the model by subtracting an estimate of the
  // model noise power.
  constexpr size_t kTailLength = 64;
  const size_t tail_index =
      GetTimeDomainLength(config_.filter.main.length_blocks) - kTailLength;
  const float tail_power = *std::max_element(matching_data.begin() + tail_index,
                                             matching_data.end());
  std::for_each(matching_data.begin(), matching_data.begin() + tail_index,
                [tail_power](float& a) { a = std::max(0.f, a - tail_power); });

  // Identify the peak index of the impulse response.
  const size_t peak_index = *std::max_element(
      matching_data.begin(), matching_data.begin() + tail_index);

  if (peak_index + 128 < tail_index) {
    size_t start_index = peak_index + 64;
    // Compute the matching residual error for the current candidate to match.
    float residual_sqr_sum = 0.f;
    float d_k = reverb_decay_to_test_;
    for (size_t k = start_index; k < tail_index; ++k) {
      if (matching_data[start_index + 1] == 0.f) {
        break;
      }

      float residual = matching_data[k] - matching_data[peak_index] * d_k;
      residual_sqr_sum += residual * residual;
      d_k *= reverb_decay_to_test_;
    }

    // If needed, update the best candidate for the reverb decay.
    if (reverb_decay_candidate_residual_ < 0.f ||
        residual_sqr_sum < reverb_decay_candidate_residual_) {
      reverb_decay_candidate_residual_ = residual_sqr_sum;
      reverb_decay_candidate_ = reverb_decay_to_test_;
    }
  }

  // Compute the next reverb candidate to evaluate such that all candidates will
  // be evaluated within one second.
  reverb_decay_to_test_ += (0.9965f - 0.9f) / (5 * kNumBlocksPerSecond);

  // If all reverb candidates have been evaluated, choose the best one as the
  // reverb decay.
  if (reverb_decay_to_test_ >= 0.9965f) {
    if (reverb_decay_candidate_residual_ < 0.f) {
      // Transform the decay to be in the unit of blocks.
      reverb_decay_ = powf(reverb_decay_candidate_, kFftLengthBy2);

      // Limit the estimated reverb_decay_ to the maximum one needed in practice
      // to minimize the impact of incorrect estimates.
      reverb_decay_ = std::min(config_.ep_strength.default_len, reverb_decay_);
    }
    reverb_decay_to_test_ = 0.9f;
    reverb_decay_candidate_residual_ = -1.f;
  }

  // For noisy impulse responses, assume a fixed tail length.
  if (tail_power > 0.0005f) {
    reverb_decay_ = config_.ep_strength.default_len;
  }
  data_dumper_->DumpRaw("aec3_reverb_decay", reverb_decay_);
  data_dumper_->DumpRaw("aec3_reverb_tail_power", tail_power);
  data_dumper_->DumpRaw("aec3_suppression_gain_limit", SuppressionGainLimit());
}

bool AecState::DetectActiveRender(rtc::ArrayView<const float> x) const {
  const float x_energy = std::inner_product(x.begin(), x.end(), x.begin(), 0.f);
  return x_energy > (config_.render_levels.active_render_limit *
                     config_.render_levels.active_render_limit) *
                        kFftLengthBy2;
}

bool AecState::DetectEchoSaturation(rtc::ArrayView<const float> x) {
  RTC_DCHECK_LT(0, x.size());
  const float max_sample = fabs(*std::max_element(
      x.begin(), x.end(), [](float a, float b) { return a * a < b * b; }));
  previous_max_sample_ = max_sample;

  // Set flag for potential presence of saturated echo
  blocks_since_last_saturation_ =
      previous_max_sample_ > 200.f && SaturatedCapture()
          ? 0
          : blocks_since_last_saturation_ + 1;

  return blocks_since_last_saturation_ < 20;
}

}  // namespace webrtc
