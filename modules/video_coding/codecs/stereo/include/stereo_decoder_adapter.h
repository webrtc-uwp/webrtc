/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_CODECS_ALPHA_INCLUDE_ALPHA_DECODER_ADAPTER_H_
#define WEBRTC_MODULES_VIDEO_CODING_CODECS_ALPHA_INCLUDE_ALPHA_DECODER_ADAPTER_H_

#include <map>
#include <memory>
#include <vector>

#include "api/video_codecs/video_decoder.h"
#include "modules/video_coding/codecs/stereo/include/stereo_encoder_adapter.h"

namespace webrtc {

class StereoDecoderAdapter : public VideoDecoder {
 public:
  explicit StereoDecoderAdapter(VideoDecoderFactoryEx* factory);
  virtual ~StereoDecoderAdapter();

  // Implements VideoDecoder
  int32_t InitDecode(const VideoCodec* codec_settings,
                     int32_t number_of_cores) override;
  int32_t Decode(const EncodedImage& input_image,
                 bool missing_frames,
                 const RTPFragmentationHeader* fragmentation,
                 const CodecSpecificInfo* codec_specific_info = NULL,
                 int64_t render_time_ms = -1) override;
  int32_t RegisterDecodeCompleteCallback(
      DecodedImageCallback* callback) override;
  int32_t Release() override;

  void Decoded(StereoCodecStream stream_idx,
               VideoFrame& decoded_image,
               rtc::Optional<int32_t> decode_time_ms,
               rtc::Optional<uint8_t> qp);

 private:
  // Wrapper class that redirects Decoded() calls.
  class AdapterDecodedImageCallback;

  // Holds the decoded image output of a frame.
  struct DecodedImageData;

  void MergeDecodedImages(VideoFrame& decoded_image,
                          const rtc::Optional<int32_t>& decode_time_ms,
                          const rtc::Optional<uint8_t>& qp,
                          VideoFrame& stereo_decoded_image,
                          const rtc::Optional<int32_t>& stereo_decode_time_ms,
                          const rtc::Optional<uint8_t>& stereo_qp);

  std::unique_ptr<VideoDecoderFactoryEx> factory_;
  std::vector<VideoDecoder*> decoders_;
  std::vector<std::unique_ptr<AdapterDecodedImageCallback>> adapter_callbacks_;
  DecodedImageCallback* decoded_complete_callback_;

  // Holds YUV or AXX decode output of a frame that is identified by timestamp.
  std::map<uint32_t /* timestamp */, DecodedImageData> decoded_data_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_CODECS_ALPHA_INCLUDE_ALPHA_DECODER_ADAPTER_H_
