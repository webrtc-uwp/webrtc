/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_CODECS_H264_H264_SIMULCAST_UNITTEST_H_
#define WEBRTC_MODULES_VIDEO_CODING_CODECS_H264_H264_SIMULCAST_UNITTEST_H_

#include <algorithm>
#include <map>
#include <memory>
#include <vector>

#include "webrtc/api/video/i420_buffer.h"
#include "webrtc/api/video/video_frame.h"
#include "webrtc/base/checks.h"
#include "webrtc/common_video/include/video_frame.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/video_coding/codecs/h264/include/h264.h"
#include "webrtc/modules/video_coding/codecs/h264/include/h264_globals.h"
#include "webrtc/modules/video_coding/include/mock/mock_video_codec_interface.h"
#include "webrtc/modules/video_coding/utility/simulcast_rate_allocator.h"
#include "webrtc/modules/video_coding/utility/simulcast_unittest_common.h"
#include "webrtc/test/gtest.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::Field;
using ::testing::Return;

namespace webrtc {
namespace testing {

class H264TestEncodedImageCallback : public EncodedImageCallback {
 public:
  H264TestEncodedImageCallback() {}

  ~H264TestEncodedImageCallback() {
    delete[] encoded_key_frame_._buffer;
    delete[] encoded_frame_._buffer;
  }

  virtual Result OnEncodedImage(const EncodedImage& encoded_image,
                                const CodecSpecificInfo* codec_specific_info,
                                const RTPFragmentationHeader* fragmentation) {
    // Only store the base layer.
    if (codec_specific_info->codecSpecific.H264.simulcastIdx == 0) {
      if (encoded_image._frameType == kVideoFrameKey) {
        delete[] encoded_key_frame_._buffer;
        encoded_key_frame_._buffer = new uint8_t[encoded_image._size];
        encoded_key_frame_._size = encoded_image._size;
        encoded_key_frame_._length = encoded_image._length;
        encoded_key_frame_._frameType = kVideoFrameKey;
        encoded_key_frame_._completeFrame = encoded_image._completeFrame;
        memcpy(encoded_key_frame_._buffer, encoded_image._buffer,
               encoded_image._length);
      } else {
        delete[] encoded_frame_._buffer;
        encoded_frame_._buffer = new uint8_t[encoded_image._size];
        encoded_frame_._size = encoded_image._size;
        encoded_frame_._length = encoded_image._length;
        memcpy(encoded_frame_._buffer, encoded_image._buffer,
               encoded_image._length);
      }
    }
    return Result(Result::OK, encoded_image._timeStamp);
  }
  void GetLastEncodedKeyFrame(EncodedImage* encoded_key_frame) {
    *encoded_key_frame = encoded_key_frame_;
  }
  void GetLastEncodedFrame(EncodedImage* encoded_frame) {
    *encoded_frame = encoded_frame_;
  }

 private:
  EncodedImage encoded_key_frame_;
  EncodedImage encoded_frame_;
};

class H264TestDecodedImageCallback : public DecodedImageCallback {
 public:
  H264TestDecodedImageCallback() : decoded_frames_(0) {}
  int32_t Decoded(VideoFrame& decoded_image) override {
    for (int i = 0; i < decoded_image.width(); ++i) {
      EXPECT_NEAR(kColorY, decoded_image.video_frame_buffer()->DataY()[i], 1);
    }

    // TODO(mikhal): Verify the difference between U,V and the original.
    for (int i = 0; i < ((decoded_image.width() + 1) / 2); ++i) {
      EXPECT_NEAR(kColorU, decoded_image.video_frame_buffer()->DataU()[i], 4);
      EXPECT_NEAR(kColorV, decoded_image.video_frame_buffer()->DataV()[i], 4);
    }
    decoded_frames_++;
    return 0;
  }
  int32_t Decoded(VideoFrame& decoded_image, int64_t decode_time_ms) override {
    RTC_NOTREACHED();
    return -1;
  }
  void Decoded(VideoFrame& decoded_image,
               rtc::Optional<int32_t> decode_time_ms,
               rtc::Optional<uint8_t> qp) override {
    Decoded(decoded_image);
  }
  int DecodedFrames() { return decoded_frames_; }

 private:
  int decoded_frames_;
};

class TestH264Simulcast : public ::testing::Test {
 public:
  TestH264Simulcast(VideoEncoder* encoder, H264Decoder* decoder)
      : encoder_(encoder), decoder_(decoder) {}

  static void SetPlane(uint8_t* data,
                       uint8_t value,
                       int width,
                       int height,
                       int stride) {
    for (int i = 0; i < height; i++, data += stride) {
      // Setting allocated area to zero - setting only image size to
      // requested values - will make it easier to distinguish between image
      // size and frame size (accounting for stride).
      memset(data, value, width);
      memset(data + width, 0, stride - width);
    }
  }

  // Fills in an I420Buffer from |plane_colors|.
  static void CreateImage(const rtc::scoped_refptr<I420Buffer>& buffer,
                          int plane_colors[kNumOfPlanes]) {
    int width = buffer->width();
    int height = buffer->height();
    int chroma_width = (width + 1) / 2;
    int chroma_height = (height + 1) / 2;

    SetPlane(buffer->MutableDataY(), plane_colors[0], width, height,
             buffer->StrideY());

    SetPlane(buffer->MutableDataU(), plane_colors[1], chroma_width,
             chroma_height, buffer->StrideU());

    SetPlane(buffer->MutableDataV(), plane_colors[2], chroma_width,
             chroma_height, buffer->StrideV());
  }

  static void DefaultSettings(VideoCodec* settings) {
    RTC_CHECK(settings);
    memset(settings, 0, sizeof(VideoCodec));
    strncpy(settings->plName, "H264", 4);
    settings->codecType = kVideoCodecH264;
    // 96 to 127 dynamic payload types for video codecs
    settings->plType = 126;
    settings->startBitrate = 300;
    settings->minBitrate = 30;
    settings->maxBitrate = 0;
    settings->maxFramerate = 30;
    settings->width = kDefaultWidth;
    settings->height = kDefaultHeight;
    settings->numberOfSimulcastStreams = kNumberOfSimulcastStreams;
    ASSERT_EQ(3, kNumberOfSimulcastStreams);
    ConfigureStream(kDefaultWidth / 4, kDefaultHeight / 4, kMaxBitrates[0],
                    kMinBitrates[0], kTargetBitrates[0],
                    &settings->simulcastStream[0]);
    ConfigureStream(kDefaultWidth / 2, kDefaultHeight / 2, kMaxBitrates[1],
                    kMinBitrates[1], kTargetBitrates[1],
                    &settings->simulcastStream[1]);
    ConfigureStream(kDefaultWidth, kDefaultHeight, kMaxBitrates[2],
                    kMinBitrates[2], kTargetBitrates[2],
                    &settings->simulcastStream[2]);
    settings->H264()->frameDroppingOn = true;
    settings->H264()->keyFrameInterval = 3000;
  }

  static void ConfigureStream(int width,
                              int height,
                              int max_bitrate,
                              int min_bitrate,
                              int target_bitrate,
                              SimulcastStream* stream) {
    assert(stream);
    stream->width = width;
    stream->height = height;
    stream->maxBitrate = max_bitrate;
    stream->minBitrate = min_bitrate;
    stream->targetBitrate = target_bitrate;
    stream->qpMax = 45;
  }

 protected:
  void SetUp() override { SetUpCodec(); }

  void TearDown() override {
    encoder_->Release();
    decoder_->Release();
  }

  void SetUpCodec() {
    encoder_->RegisterEncodeCompleteCallback(&encoder_callback_);
    decoder_->RegisterDecodeCompleteCallback(&decoder_callback_);
    DefaultSettings(&settings_);
    SetUpRateAllocator();
    EXPECT_EQ(0, encoder_->InitEncode(&settings_, 1, 1200));
    EXPECT_EQ(0, decoder_->InitDecode(&settings_, 1));
    int half_width = (kDefaultWidth + 1) / 2;
    input_buffer_ = I420Buffer::Create(kDefaultWidth, kDefaultHeight,
                                       kDefaultWidth, half_width, half_width);
    input_buffer_->InitializeData();
    input_frame_.reset(
        new VideoFrame(input_buffer_, 0, 0, webrtc::kVideoRotation_0));
  }

  void SetUpRateAllocator() {
    TemporalLayersFactory* tl_factory = new TemporalLayersFactory();
    rate_allocator_.reset(new SimulcastRateAllocator(
        settings_, std::unique_ptr<TemporalLayersFactory>(tl_factory)));
  }

  void SetRates(uint32_t bitrate_kbps, uint32_t fps) {
    encoder_->SetRateAllocation(
        rate_allocator_->GetAllocation(bitrate_kbps * 1000, fps), fps);
  }

  void ExpectStreams(FrameType frame_type, int expected_video_streams) {
    ASSERT_GE(expected_video_streams, 0);
    ASSERT_LE(expected_video_streams, kNumberOfSimulcastStreams);
    if (expected_video_streams >= 1) {
      EXPECT_CALL(
          encoder_callback_,
          OnEncodedImage(
              AllOf(Field(&EncodedImage::_frameType, frame_type),
                    Field(&EncodedImage::_encodedWidth, kDefaultWidth / 4),
                    Field(&EncodedImage::_encodedHeight, kDefaultHeight / 4)),
              _, _))
          .Times(1)
          .WillRepeatedly(Return(EncodedImageCallback::Result(
              EncodedImageCallback::Result::OK, 0)));
    }
    if (expected_video_streams >= 2) {
      EXPECT_CALL(
          encoder_callback_,
          OnEncodedImage(
              AllOf(Field(&EncodedImage::_frameType, frame_type),
                    Field(&EncodedImage::_encodedWidth, kDefaultWidth / 2),
                    Field(&EncodedImage::_encodedHeight, kDefaultHeight / 2)),
              _, _))
          .Times(1)
          .WillRepeatedly(Return(EncodedImageCallback::Result(
              EncodedImageCallback::Result::OK, 0)));
    }
    if (expected_video_streams >= 3) {
      EXPECT_CALL(
          encoder_callback_,
          OnEncodedImage(
              AllOf(Field(&EncodedImage::_frameType, frame_type),
                    Field(&EncodedImage::_encodedWidth, kDefaultWidth),
                    Field(&EncodedImage::_encodedHeight, kDefaultHeight)),
              _, _))
          .Times(1)
          .WillRepeatedly(Return(EncodedImageCallback::Result(
              EncodedImageCallback::Result::OK, 0)));
    }
  }

  // We currently expect all active streams to generate a key frame even though
  // a key frame was only requested for some of them.
  void TestKeyFrameRequestsOnAllStreams() {
    SetRates(kMaxBitrates[2], 30);  // To get all three streams.
    std::vector<FrameType> frame_types(kNumberOfSimulcastStreams,
                                       kVideoFrameDelta);
    ExpectStreams(kVideoFrameKey, kNumberOfSimulcastStreams);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));

    ExpectStreams(kVideoFrameDelta, kNumberOfSimulcastStreams);
    input_frame_->set_timestamp(input_frame_->timestamp() + 3000);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));

    frame_types[0] = kVideoFrameKey;
    ExpectStreams(kVideoFrameKey, kNumberOfSimulcastStreams);
    input_frame_->set_timestamp(input_frame_->timestamp() + 3000);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));

    std::fill(frame_types.begin(), frame_types.end(), kVideoFrameDelta);
    frame_types[1] = kVideoFrameKey;
    ExpectStreams(kVideoFrameKey, kNumberOfSimulcastStreams);
    input_frame_->set_timestamp(input_frame_->timestamp() + 3000);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));

    std::fill(frame_types.begin(), frame_types.end(), kVideoFrameDelta);
    frame_types[2] = kVideoFrameKey;
    ExpectStreams(kVideoFrameKey, kNumberOfSimulcastStreams);
    input_frame_->set_timestamp(input_frame_->timestamp() + 3000);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));

    std::fill(frame_types.begin(), frame_types.end(), kVideoFrameDelta);
    ExpectStreams(kVideoFrameDelta, kNumberOfSimulcastStreams);
    input_frame_->set_timestamp(input_frame_->timestamp() + 3000);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));
  }

  void TestPaddingAllStreams() {
    // We should always encode the base layer.
    SetRates(kMinBitrates[0] - 1, 30);
    std::vector<FrameType> frame_types(kNumberOfSimulcastStreams,
                                       kVideoFrameDelta);
    ExpectStreams(kVideoFrameKey, 1);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));

    ExpectStreams(kVideoFrameDelta, 1);
    input_frame_->set_timestamp(input_frame_->timestamp() + 3000);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));
  }

  void TestPaddingTwoStreams() {
    // We have just enough to get only the first stream and padding for two.
    SetRates(kMinBitrates[0], 30);
    std::vector<FrameType> frame_types(kNumberOfSimulcastStreams,
                                       kVideoFrameDelta);
    ExpectStreams(kVideoFrameKey, 1);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));

    ExpectStreams(kVideoFrameDelta, 1);
    input_frame_->set_timestamp(input_frame_->timestamp() + 3000);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));
  }

  void TestPaddingTwoStreamsOneMaxedOut() {
    // We are just below limit of sending second stream, so we should get
    // the first stream maxed out (at |maxBitrate|), and padding for two.
    SetRates(kTargetBitrates[0] + kMinBitrates[1] - 1, 30);
    std::vector<FrameType> frame_types(kNumberOfSimulcastStreams,
                                       kVideoFrameDelta);
    ExpectStreams(kVideoFrameKey, 1);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));

    ExpectStreams(kVideoFrameDelta, 1);
    input_frame_->set_timestamp(input_frame_->timestamp() + 3000);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));
  }

  void TestPaddingOneStream() {
    // We have just enough to send two streams, so padding for one stream.
    SetRates(kTargetBitrates[0] + kMinBitrates[1], 30);
    std::vector<FrameType> frame_types(kNumberOfSimulcastStreams,
                                       kVideoFrameDelta);
    ExpectStreams(kVideoFrameKey, 2);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));

    ExpectStreams(kVideoFrameDelta, 2);
    input_frame_->set_timestamp(input_frame_->timestamp() + 3000);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));
  }

  void TestPaddingOneStreamTwoMaxedOut() {
    // We are just below limit of sending third stream, so we should get
    // first stream's rate maxed out at |targetBitrate|, second at |maxBitrate|.
    SetRates(kTargetBitrates[0] + kTargetBitrates[1] + kMinBitrates[2] - 1, 30);
    std::vector<FrameType> frame_types(kNumberOfSimulcastStreams,
                                       kVideoFrameDelta);
    ExpectStreams(kVideoFrameKey, 2);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));

    ExpectStreams(kVideoFrameDelta, 2);
    input_frame_->set_timestamp(input_frame_->timestamp() + 3000);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));
  }

  void TestSendAllStreams() {
    // We have just enough to send all streams.
    SetRates(kTargetBitrates[0] + kTargetBitrates[1] + kMinBitrates[2], 30);
    std::vector<FrameType> frame_types(kNumberOfSimulcastStreams,
                                       kVideoFrameDelta);
    ExpectStreams(kVideoFrameKey, 3);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));

    ExpectStreams(kVideoFrameDelta, 3);
    input_frame_->set_timestamp(input_frame_->timestamp() + 3000);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));
  }

  void TestDisablingStreams() {
    // We should get three media streams.
    SetRates(kMaxBitrates[0] + kMaxBitrates[1] + kMaxBitrates[2], 30);
    std::vector<FrameType> frame_types(kNumberOfSimulcastStreams,
                                       kVideoFrameDelta);
    ExpectStreams(kVideoFrameKey, 3);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));

    ExpectStreams(kVideoFrameDelta, 3);
    input_frame_->set_timestamp(input_frame_->timestamp() + 3000);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));

    // We should only get two streams and padding for one.
    SetRates(kTargetBitrates[0] + kTargetBitrates[1] + kMinBitrates[2] / 2, 30);
    ExpectStreams(kVideoFrameDelta, 2);
    input_frame_->set_timestamp(input_frame_->timestamp() + 3000);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));

    // We should only get the first stream and padding for two.
    SetRates(kTargetBitrates[0] + kMinBitrates[1] / 2, 30);
    ExpectStreams(kVideoFrameDelta, 1);
    input_frame_->set_timestamp(input_frame_->timestamp() + 3000);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));

    // We don't have enough bitrate for the thumbnail stream, but we should get
    // it anyway with current configuration.
    SetRates(kTargetBitrates[0] - 1, 30);
    ExpectStreams(kVideoFrameDelta, 1);
    input_frame_->set_timestamp(input_frame_->timestamp() + 3000);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));

    // We should only get two streams and padding for one.
    SetRates(kTargetBitrates[0] + kTargetBitrates[1] + kMinBitrates[2] / 2, 30);
    // We get a key frame because a new stream is being enabled.
    ExpectStreams(kVideoFrameKey, 2);
    input_frame_->set_timestamp(input_frame_->timestamp() + 3000);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));

    // We should get all three streams.
    SetRates(kTargetBitrates[0] + kTargetBitrates[1] + kTargetBitrates[2], 30);
    // We get a key frame because a new stream is being enabled.
    ExpectStreams(kVideoFrameKey, 3);
    input_frame_->set_timestamp(input_frame_->timestamp() + 3000);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));
  }

  void SwitchingToOneStream(int width, int height) {
    // Disable all streams except the last and set the bitrate of the last to
    // 100 kbps. This verifies the way GTP switches to screenshare mode.
    settings_.maxBitrate = 100;
    settings_.startBitrate = 100;
    settings_.width = width;
    settings_.height = height;
    for (int i = 0; i < settings_.numberOfSimulcastStreams - 1; ++i) {
      settings_.simulcastStream[i].maxBitrate = 0;
      settings_.simulcastStream[i].width = settings_.width;
      settings_.simulcastStream[i].height = settings_.height;
    }
    // Setting input image to new resolution.
    int half_width = (settings_.width + 1) / 2;
    input_buffer_ = I420Buffer::Create(settings_.width, settings_.height,
                                       settings_.width, half_width, half_width);
    input_buffer_->InitializeData();

    input_frame_.reset(
        new VideoFrame(input_buffer_, 0, 0, webrtc::kVideoRotation_0));

    // The for loop above did not set the bitrate of the highest layer.
    settings_.simulcastStream[settings_.numberOfSimulcastStreams - 1]
        .maxBitrate = 0;
    // The highest layer has to correspond to the non-simulcast resolution.
    settings_.simulcastStream[settings_.numberOfSimulcastStreams - 1].width =
        settings_.width;
    settings_.simulcastStream[settings_.numberOfSimulcastStreams - 1].height =
        settings_.height;
    SetUpRateAllocator();
    EXPECT_EQ(0, encoder_->InitEncode(&settings_, 1, 1200));

    // Encode one frame and verify.
    SetRates(kMaxBitrates[0] + kMaxBitrates[1], 30);
    std::vector<FrameType> frame_types(kNumberOfSimulcastStreams,
                                       kVideoFrameDelta);
    EXPECT_CALL(
        encoder_callback_,
        OnEncodedImage(AllOf(Field(&EncodedImage::_frameType, kVideoFrameKey),
                             Field(&EncodedImage::_encodedWidth, width),
                             Field(&EncodedImage::_encodedHeight, height)),
                       _, _))
        .Times(1)
        .WillRepeatedly(Return(
            EncodedImageCallback::Result(EncodedImageCallback::Result::OK, 0)));
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));

    // Switch back.
    DefaultSettings(&settings_);
    // Start at the lowest bitrate for enabling base stream.
    settings_.startBitrate = kMinBitrates[0];
    SetUpRateAllocator();
    EXPECT_EQ(0, encoder_->InitEncode(&settings_, 1, 1200));
    SetRates(settings_.startBitrate, 30);
    ExpectStreams(kVideoFrameKey, 1);
    // Resize |input_frame_| to the new resolution.
    half_width = (settings_.width + 1) / 2;
    input_buffer_ = I420Buffer::Create(settings_.width, settings_.height,
                                       settings_.width, half_width, half_width);
    input_buffer_->InitializeData();
    input_frame_.reset(
        new VideoFrame(input_buffer_, 0, 0, webrtc::kVideoRotation_0));
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, &frame_types));
  }

  void TestSwitchingToOneStream() { SwitchingToOneStream(1024, 768); }

  void TestSwitchingToOneOddStream() { SwitchingToOneStream(1023, 769); }

  void TestSwitchingToOneSmallStream() { SwitchingToOneStream(4, 4); }

  void TestStrideEncodeDecode() {
    H264TestEncodedImageCallback encoder_callback;
    H264TestDecodedImageCallback decoder_callback;
    encoder_->RegisterEncodeCompleteCallback(&encoder_callback);
    decoder_->RegisterDecodeCompleteCallback(&decoder_callback);

    SetRates(kMaxBitrates[2], 30);  // To get all three streams.
    // Setting two (possibly) problematic use cases for stride:
    // 1. stride > width 2. stride_y != stride_uv/2
    int stride_y = kDefaultWidth + 20;
    int stride_uv = ((kDefaultWidth + 1) / 2) + 5;
    input_buffer_ = I420Buffer::Create(kDefaultWidth, kDefaultHeight, stride_y,
                                       stride_uv, stride_uv);
    input_frame_.reset(
        new VideoFrame(input_buffer_, 0, 0, webrtc::kVideoRotation_0));

    // Set color.
    int plane_offset[kNumOfPlanes];
    plane_offset[kYPlane] = kColorY;
    plane_offset[kUPlane] = kColorU;
    plane_offset[kVPlane] = kColorV;
    CreateImage(input_buffer_, plane_offset);

    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, NULL));

    // Change color.
    plane_offset[kYPlane] += 1;
    plane_offset[kUPlane] += 1;
    plane_offset[kVPlane] += 1;
    CreateImage(input_buffer_, plane_offset);
    input_frame_->set_timestamp(input_frame_->timestamp() + 3000);
    EXPECT_EQ(0, encoder_->Encode(*input_frame_, NULL, NULL));

    EncodedImage encoded_frame;
    // Only encoding one frame - so will be a key frame.
    encoder_callback.GetLastEncodedKeyFrame(&encoded_frame);
    EXPECT_EQ(0, decoder_->Decode(encoded_frame, false, NULL));
    encoder_callback.GetLastEncodedFrame(&encoded_frame);
    decoder_->Decode(encoded_frame, false, NULL);
    EXPECT_EQ(2, decoder_callback.DecodedFrames());
  }

  std::unique_ptr<VideoEncoder> encoder_;
  MockEncodedImageCallback encoder_callback_;
  std::unique_ptr<H264Decoder> decoder_;
  MockDecodedImageCallback decoder_callback_;
  VideoCodec settings_;
  rtc::scoped_refptr<I420Buffer> input_buffer_;
  std::unique_ptr<VideoFrame> input_frame_;
  std::unique_ptr<SimulcastRateAllocator> rate_allocator_;
};

}  // namespace testing
}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_CODECS_H264_H264_SIMULCAST_UNITTEST_H_
