/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/stereo/include/stereo_decoder_adapter.h"

#include "api/video/i420_buffer.h"
#include "common_video/include/video_frame.h"
#include "common_video/include/video_frame_buffer.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "rtc_base/keep_ref_until_done.h"
#include "rtc_base/logging.h"

namespace webrtc {

class StereoDecoderAdapter::AdapterDecodedImageCallback
    : public webrtc::DecodedImageCallback {
 public:
  AdapterDecodedImageCallback(webrtc::StereoDecoderAdapter* adapter,
                              StereoCodecStream stream_idx)
      : adapter_(adapter), stream_idx_(stream_idx) {}

  void Decoded(VideoFrame& decodedImage,
               rtc::Optional<int32_t> decode_time_ms,
               rtc::Optional<uint8_t> qp) override {
    if (!adapter_)
      return;
    adapter_->Decoded(stream_idx_, decodedImage, decode_time_ms, qp);
  }
  int32_t Decoded(VideoFrame& decodedImage) override {
    RTC_NOTREACHED();
    return WEBRTC_VIDEO_CODEC_OK;
  }
  int32_t Decoded(VideoFrame& decodedImage, int64_t decode_time_ms) override {
    RTC_NOTREACHED();
    return WEBRTC_VIDEO_CODEC_OK;
  }

 private:
  StereoDecoderAdapter* adapter_;
  const StereoCodecStream stream_idx_;
};

struct StereoDecoderAdapter::DecodedImageData {
  explicit DecodedImageData(StereoCodecStream stream_idx)
      : stream_idx_(stream_idx),
        decodedImage_(I420Buffer::Create(1 /* width */, 1 /* height */),
                      0,
                      0,
                      kVideoRotation_0) {
    RTC_DCHECK_EQ(kAXXStream, stream_idx);
  }
  DecodedImageData(StereoCodecStream stream_idx,
                   const VideoFrame& decodedImage,
                   const rtc::Optional<int32_t>& decode_time_ms,
                   const rtc::Optional<uint8_t>& qp)
      : stream_idx_(stream_idx),
        decodedImage_(decodedImage),
        decode_time_ms_(decode_time_ms),
        qp_(qp) {}
  const StereoCodecStream stream_idx_;
  VideoFrame decodedImage_;
  const rtc::Optional<int32_t> decode_time_ms_;
  const rtc::Optional<uint8_t> qp_;

 private:
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(DecodedImageData);
};

StereoDecoderAdapter::StereoDecoderAdapter(VideoDecoderFactoryEx* factory)
    : factory_(factory) {}

StereoDecoderAdapter::~StereoDecoderAdapter() {
  Release();
}

int32_t StereoDecoderAdapter::InitDecode(const VideoCodec* codec_settings,
                                         int32_t number_of_cores) {
  VideoCodec settings = *codec_settings;
  settings.codecType = kVideoCodecVP9;
  for (size_t i = 0; i < kStereoCodecStreams; ++i) {
    VideoDecoder* decoder = factory_->Create();
    const int32_t rv = decoder->InitDecode(&settings, number_of_cores);
    if (rv)
      return rv;
    decoders_.push_back(decoder);
    adapter_callbacks_.emplace_back(
        new StereoDecoderAdapter::AdapterDecodedImageCallback(
            this, static_cast<StereoCodecStream>(i)));
    decoder->RegisterDecodeCompleteCallback(adapter_callbacks_.back().get());
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t StereoDecoderAdapter::Decode(
    const EncodedImage& input_image,
    bool missing_frames,
    const RTPFragmentationHeader* /*fragmentation*/,
    const CodecSpecificInfo* codec_specific_info,
    int64_t render_time_ms) {
  LOG(LS_ERROR) << __func__;
  LOG(LS_ERROR) << __func__
                << static_cast<int>(codec_specific_info->stereoInfo.frameIndex);
  LOG(LS_ERROR) << __func__
                << static_cast<int>(codec_specific_info->stereoInfo.frameCount);
  LOG(LS_ERROR) << __func__
                << static_cast<int>(
                       codec_specific_info->stereoInfo.pictureIndex);
  if (codec_specific_info->stereoInfo.frameCount == 1) {
    RTC_DCHECK(decoded_data_.find(input_image._timeStamp) ==
               decoded_data_.end());
    decoded_data_.emplace(std::piecewise_construct,
                          std::forward_as_tuple(input_image._timeStamp),
                          std::forward_as_tuple(kAXXStream));
  }

  int32_t rv = decoders_[codec_specific_info->stereoInfo.frameIndex]->Decode(
      input_image, missing_frames, nullptr, codec_specific_info,
      render_time_ms);
  return rv;
}

int32_t StereoDecoderAdapter::RegisterDecodeCompleteCallback(
    DecodedImageCallback* callback) {
  decoded_complete_callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t StereoDecoderAdapter::Release() {
  for (auto decoder : decoders_) {
    const int32_t rv = decoder->Release();
    if (rv)
      return rv;
    factory_->Destroy(decoder);
  }
  decoders_.clear();
  adapter_callbacks_.clear();
  return WEBRTC_VIDEO_CODEC_OK;
}

void StereoDecoderAdapter::Decoded(StereoCodecStream stream_idx,
                                   VideoFrame& decoded_image,
                                   rtc::Optional<int32_t> decode_time_ms,
                                   rtc::Optional<uint8_t> qp) {
  const auto& other_decoded_data_it =
      decoded_data_.find(decoded_image.timestamp());
  if (other_decoded_data_it != decoded_data_.end()) {
    auto& other_image_data = other_decoded_data_it->second;
    if (stream_idx == kYUVStream) {
      RTC_DCHECK_EQ(kAXXStream, other_image_data.stream_idx_);
      MergeDecodedImages(
          decoded_image, decode_time_ms, qp, other_image_data.decodedImage_,
          other_image_data.decode_time_ms_, other_image_data.qp_);
    } else {
      RTC_DCHECK_EQ(kYUVStream, other_image_data.stream_idx_);
      RTC_DCHECK_EQ(kAXXStream, stream_idx);
      MergeDecodedImages(other_image_data.decodedImage_,
                         other_image_data.decode_time_ms_, other_image_data.qp_,
                         decoded_image, decode_time_ms, qp);
    }
    decoded_data_.erase(decoded_data_.begin(), other_decoded_data_it);
    return;
  }
  RTC_DCHECK(decoded_data_.find(decoded_image.timestamp()) ==
             decoded_data_.end());
  decoded_data_.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(decoded_image.timestamp()),
      std::forward_as_tuple(stream_idx, decoded_image, decode_time_ms, qp));
}

void StereoDecoderAdapter::MergeDecodedImages(
    VideoFrame& decodedImage,
    const rtc::Optional<int32_t>& decode_time_ms,
    const rtc::Optional<uint8_t>& qp,
    VideoFrame& alpha_decodedImage,
    const rtc::Optional<int32_t>& stereo_decode_time_ms,
    const rtc::Optional<uint8_t>& stereo_qp) {
  if (!alpha_decodedImage.timestamp()) {
    decoded_complete_callback_->Decoded(decodedImage, decode_time_ms, qp);
    return;
  }
  rtc::scoped_refptr<webrtc::I420BufferInterface> alpha_buffer =
      alpha_decodedImage.video_frame_buffer()->ToI420();
  rtc::scoped_refptr<WrappedI420ABuffer> wrapped_buffer(
      new rtc::RefCountedObject<webrtc::WrappedI420ABuffer>(
          decodedImage.video_frame_buffer(), alpha_buffer->DataY(),
          alpha_buffer->StrideY(),
          rtc::KeepRefUntilDone(alpha_decodedImage.video_frame_buffer())));
  VideoFrame wrapped_image(wrapped_buffer, decodedImage.timestamp(),
                           0 /* render_time_ms */, decodedImage.rotation());
  decoded_complete_callback_->Decoded(wrapped_image, decode_time_ms, qp);
}

}  // namespace webrtc
