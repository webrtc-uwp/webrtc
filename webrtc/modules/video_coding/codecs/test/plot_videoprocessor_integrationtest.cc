/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/codecs/test/videoprocessor_integrationtest.h"

namespace webrtc {
namespace test {

namespace {
// Codec settings.
const int kNumFrames = 800;
const int kBitrates[] = {64, 96, 128, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 1200, 1400, 1700};
const bool kErrorConcealmentOn = false;
const bool kDenoisingOn = true;
const bool kFrameDropperOn = false;
const bool kSpatialResizeOn = false;
const VideoCodecType kVideoCodecType[] = {kVideoCodecVP9, kVideoCodecH264};
const bool kHwCodec[] = {true};
const bool kUseSingleCore = false;

// Test settings.
const bool kBatchMode = true;
const bool kCalculatePsnrAndSsim = true;

// Packet loss probability [0.0, 1.0].
const float kPacketLoss = 0.0f;

const VisualizationParams kVisualizationParams = {
    false,  // save_source_y4m
    true,  // save_encoded_ivf
    false,   // save_decoded_y4m
};

const bool kVerboseLogging = true;
}  // namespace

// Tests for plotting statistics from logs.
class PlotVideoProcessorIntegrationTest
    : public VideoProcessorIntegrationTest,
      public ::testing::WithParamInterface<
          ::testing::tuple<int, VideoCodecType, bool>> {
 protected:
  PlotVideoProcessorIntegrationTest()
      : bitrate_(::testing::get<0>(GetParam())),
        codec_type_(::testing::get<1>(GetParam())),
        hw_codec_(::testing::get<2>(GetParam())) {}

  virtual ~PlotVideoProcessorIntegrationTest() {}

  void RunTest(int width,
               int height,
               int frame_rate,
               const std::string& filename,
               int num_frames) {
    // Bitrate and frame rate profile.
    RateProfile rate_profile;
    SetRateProfile(&rate_profile,
                   0,  // update_index
                   bitrate_, frame_rate,
                   0);  // frame_index_rate_update
    rate_profile.frame_index_rate_update[1] = num_frames + 1;
    rate_profile.num_frames = num_frames;

    // Codec/network settings.
    CodecParams process_settings;
    SetCodecParams(&process_settings, codec_type_, hw_codec_, kUseSingleCore,
                   kPacketLoss,
                   -1,  // key_frame_interval
                   1,   // num_temporal_layers
                   kErrorConcealmentOn, kDenoisingOn, kFrameDropperOn,
                   kSpatialResizeOn, width, height, filename, kVerboseLogging,
                   kBatchMode, kCalculatePsnrAndSsim);

    // Use default thresholds for quality (PSNR and SSIM).
    QualityThresholds quality_thresholds;

    // Use very loose thresholds for rate control, so even poor HW codecs will
    // pass the requirements.
    RateControlThresholds rc_thresholds[1];
    // clang-format off
    SetRateControlThresholds(
      rc_thresholds,
      0,               // update_index
      num_frames + 1,  // max_num_dropped_frames
      10000000,           // max_key_frame_size_mismatch
      10000000,           // max_delta_frame_size_mismatch
      10000000,           // max_encoding_rate_mismatch
      num_frames + 1,  // max_time_hit_target
      -1,              // num_spatial_resizes
      -1);             // num_key_frames
    // clang-format on

    ProcessFramesAndVerify(quality_thresholds, rate_profile, process_settings,
                           rc_thresholds, &kVisualizationParams);
  }

  const int bitrate_;
  const VideoCodecType codec_type_;
  const bool hw_codec_;
};

INSTANTIATE_TEST_CASE_P(CodecSettings,
                        PlotVideoProcessorIntegrationTest,
                        ::testing::Combine(::testing::ValuesIn(kBitrates),
                                           ::testing::ValuesIn(kVideoCodecType),
                                           ::testing::ValuesIn(kHwCodec)));

TEST_P(PlotVideoProcessorIntegrationTest, Still_Bright_r360_fr30) {
  RunTest(360, 640, 30, "Still_Bright_r360_fr30", kNumFrames);
}

TEST_P(PlotVideoProcessorIntegrationTest, Still_Bright_r360_fr15) {
  RunTest(360, 640, 15, "Still_Bright_r360_fr15", kNumFrames / 2);
}

TEST_P(PlotVideoProcessorIntegrationTest, Still_Bright_r268_fr30) {
  RunTest(268, 476, 30, "Still_Bright_r268_fr30", kNumFrames);
}

TEST_P(PlotVideoProcessorIntegrationTest, Still_Bright_r268_fr15) {
  RunTest(268, 476, 15, "Still_Bright_r268_fr15", kNumFrames / 2);
}

TEST_P(PlotVideoProcessorIntegrationTest, Still_Bright_r180_fr30) {
  RunTest(180, 320, 30, "Still_Bright_r180_fr30", kNumFrames);
}

TEST_P(PlotVideoProcessorIntegrationTest, Still_Bright_r180_fr15) {
  RunTest(180, 320, 15, "Still_Bright_r180_fr15", kNumFrames / 2);
}

TEST_P(PlotVideoProcessorIntegrationTest, mac_marco_moving) {
  RunTest(640, 480, 30, "mac_marco_moving.640_480", 986);
}

}  // namespace test
}  // namespace webrtc
