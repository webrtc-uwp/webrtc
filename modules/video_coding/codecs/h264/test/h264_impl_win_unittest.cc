/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video/color_space.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "gtest/gtest-spi.h"
#include "modules/video_coding/codecs/h264/include/h264.h"
#include "modules/video_coding/codecs/test/video_codec_unittest.h"
#include "test/video_codec_settings.h"
#include "third_party/winuwp_h264/H264Decoder/H264Decoder.h"
#include "third_party/winuwp_h264/H264Encoder/H264Encoder.h"

#include <exception>

namespace webrtc {

class TestH264ImplWin : public VideoCodecUnitTest {
 protected:
  std::unique_ptr<VideoEncoder> CreateEncoder() override {
    return absl::make_unique<WinUWPH264EncoderImpl>();
  }

  std::unique_ptr<VideoDecoder> CreateDecoder() override {
    return absl::make_unique<WinUWPH264DecoderImpl>();
  }

  void ModifyCodecSettings(VideoCodec* codec_settings) override {
    webrtc::test::CodecSettings(kVideoCodecH264, codec_settings);
  }
};

TEST_F(TestH264ImplWin, CanInitializeEncoderWithDefaultParameters) {
  WinUWPH264EncoderImpl encoder = WinUWPH264EncoderImpl();
  VideoCodec codec_settings;
  ModifyCodecSettings(&codec_settings);
  EXPECT_EQ(0, encoder.InitEncode(&codec_settings, 1, 1024));
}

TEST_F(TestH264ImplWin, CanInitializeDecoderWithDefaultParameters) {
  WinUWPH264DecoderImpl decoder = WinUWPH264DecoderImpl();
  VideoCodec codec_settings;
  ModifyCodecSettings(&codec_settings);
  EXPECT_EQ(0, decoder.InitDecode(&codec_settings, 1));
}

TEST_F(TestH264ImplWin, InitEncode) {
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, encoder_->Release());
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->InitEncode(&codec_settings_, 1 /* number of cores */,
                                 0 /* max payload size (unused) */));
}

TEST_F(TestH264ImplWin, InitDecode) {
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, decoder_->Release());
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, decoder_->InitDecode(&codec_settings_, 1));
}

TEST_F(TestH264ImplWin, EncodeDecode) {
  
  bool gotFrame = false;

  for (int i = 0; i < 50000; i++) {
    
    std::vector<FrameType> frame_types { kVideoFrameKey };
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              encoder_->Encode(*NextInputFrame(), nullptr, &frame_types));
    EncodedImage encoded_frame;
    CodecSpecificInfo codec_specific_info;
    ASSERT_TRUE(WaitForEncodedFrame(&encoded_frame, &codec_specific_info));

    // First frame should be a key frame.
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
              decoder_->Decode(encoded_frame, false, nullptr, 0));
    std::unique_ptr<VideoFrame> decoded_frame;
    absl::optional<uint8_t> decoded_qp;

    gotFrame = TryWaitForDecodedFrame(&decoded_frame, &decoded_qp);

    if (gotFrame) break;
  }

  ASSERT_TRUE(gotFrame); // if gotFrame is false,
}

}  // namespace webrtc
