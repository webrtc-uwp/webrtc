/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/statistics_calculator.h"

#include "test/gtest.h"

namespace webrtc {

TEST(LifetimeStatistics, TotalSamplesReceived) {
  StatisticsCalculator stats;
  for (int i = 0; i < 10; ++i) {
    stats.IncreaseCounter(480, 48000);  // 10 ms at 48 kHz.
  }
  EXPECT_EQ(10 * 480u, stats.GetLifetimeStatistics().total_samples_received);
}

TEST(LifetimeStatistics, SamplesConcealed) {
  StatisticsCalculator stats;
  stats.ExpandedVoiceSamples(100);
  stats.ExpandedNoiseSamples(17);
  EXPECT_EQ(100u + 17u, stats.GetLifetimeStatistics().concealed_samples);
}

TEST(LifetimeStatistics, SamplesConcealedCorrection) {
  StatisticsCalculator stats;
  stats.ExpandedVoiceSamples(100);
  EXPECT_EQ(100u, stats.GetLifetimeStatistics().concealed_samples);
  stats.ExpandedVoiceSamplesCorrection(-10);
  // Do not subtract directly, but keep the correction for later.
  EXPECT_EQ(100u, stats.GetLifetimeStatistics().concealed_samples);
  stats.ExpandedVoiceSamplesCorrection(20);
  // The total correction is 20 - 10.
  EXPECT_EQ(110u, stats.GetLifetimeStatistics().concealed_samples);

  // Also test correction done to the next ExpandedVoiceSamples call.
  stats.ExpandedVoiceSamplesCorrection(-17);
  EXPECT_EQ(110u, stats.GetLifetimeStatistics().concealed_samples);
  stats.ExpandedVoiceSamples(100);
  EXPECT_EQ(110u + 100u - 17u, stats.GetLifetimeStatistics().concealed_samples);
}

TEST(LifetimeStatistics, NoUpdateOnTimeStretch) {
  StatisticsCalculator stats;
  stats.ExpandedVoiceSamples(100);
  stats.AcceleratedSamples(4711);
  stats.PreemptiveExpandedSamples(17);
  stats.ExpandedVoiceSamples(100);
  EXPECT_EQ(200u, stats.GetLifetimeStatistics().concealed_samples);
}

}  // namespace webrtc
