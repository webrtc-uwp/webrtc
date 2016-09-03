/*
*  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
*
*  Use of this source code is governed by a BSD-style license
*  that can be found in the LICENSE file in the root of the source
*  tree. An additional intellectual property rights grant can be found
*  in the file PATENTS.  All contributing project authors may
*  be found in the AUTHORS file in the root of the source tree.
*/

#ifndef THIRD_PARTY_H264_WINRT_H264_WINRT_FACTORY_H_
#define THIRD_PARTY_H264_WINRT_H264_WINRT_FACTORY_H_

#include <vector>
#include "webrtc/media/engine/webrtcvideoencoderfactory.h"
#include "webrtc/media/engine/webrtcvideodecoderfactory.h"


namespace webrtc {

class H264WinRTEncoderFactory : public cricket::WebRtcVideoEncoderFactory {
 public:
  H264WinRTEncoderFactory();

  webrtc::VideoEncoder* CreateVideoEncoder(webrtc::VideoCodecType type)
    override;

  const std::vector<cricket::WebRtcVideoEncoderFactory::VideoCodec>& codecs()
    const override;

  void DestroyVideoEncoder(webrtc::VideoEncoder* encoder) override;

 private:
  std::vector<cricket::WebRtcVideoEncoderFactory::VideoCodec> codecList_;
};

class H264WinRTDecoderFactory : public cricket::WebRtcVideoDecoderFactory {
  webrtc::VideoDecoder* CreateVideoDecoder(webrtc::VideoCodecType type)
    override;

  void DestroyVideoDecoder(webrtc::VideoDecoder* decoder) override;
};
}  // namespace webrtc

#endif  // THIRD_PARTY_H264_WINRT_H264_WINRT_FACTORY_H_
