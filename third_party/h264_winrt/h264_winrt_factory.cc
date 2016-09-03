/*
*  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
*
*  Use of this source code is governed by a BSD-style license
*  that can be found in the LICENSE file in the root of the source
*  tree. An additional intellectual property rights grant can be found
*  in the file PATENTS.  All contributing project authors may
*  be found in the AUTHORS file in the root of the source tree.
*/

#include <vector>
#include "third_party/h264_winrt/h264_winrt_factory.h"
#include "third_party/h264_winrt/H264Encoder/H264Encoder.h"
#include "third_party/h264_winrt/H264Decoder/H264Decoder.h"
#include "webrtc/media/engine/webrtcvideoencoderfactory.h"
#include "webrtc/media/engine/webrtcvideodecoderfactory.h"


namespace webrtc {

  H264WinRTEncoderFactory::H264WinRTEncoderFactory() {
    codecList_ =
      std::vector<cricket::WebRtcVideoEncoderFactory::VideoCodec> {
        cricket::WebRtcVideoEncoderFactory::VideoCodec(
        webrtc::VideoCodecType::kVideoCodecH264,
        "H264",
        1920, 1080, 60)  // Max width/height/fps
    };
  }

  webrtc::VideoEncoder* H264WinRTEncoderFactory::CreateVideoEncoder(
    webrtc::VideoCodecType type) {
    if (type == kVideoCodecH264) {
      return new H264WinRTEncoderImpl();
    } else {
      return nullptr;
    }
  }

  const std::vector<cricket::WebRtcVideoEncoderFactory::VideoCodec>&
    H264WinRTEncoderFactory::codecs() const {
    return codecList_;
  }

  void H264WinRTEncoderFactory::DestroyVideoEncoder(
    webrtc::VideoEncoder* encoder) {
      encoder->Release();
      delete encoder;
  }


  webrtc::VideoDecoder* H264WinRTDecoderFactory::CreateVideoDecoder(
    webrtc::VideoCodecType type) {
    if (type == kVideoCodecH264) {
      return new H264WinRTDecoderImpl();
    } else {
      return nullptr;
    }
  }

  void H264WinRTDecoderFactory::DestroyVideoDecoder(
    webrtc::VideoDecoder* decoder) {
    decoder->Release();
    delete decoder;
  }

}  // namespace webrtc

