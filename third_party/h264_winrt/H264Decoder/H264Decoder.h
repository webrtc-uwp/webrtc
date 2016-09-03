/*
*  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
*
*  Use of this source code is governed by a BSD-style license
*  that can be found in the LICENSE file in the root of the source
*  tree. An additional intellectual property rights grant can be found
*  in the file PATENTS.  All contributing project authors may
*  be found in the AUTHORS file in the root of the source tree.
*/

#ifndef THIRD_PARTY_H264_WINRT_H264DECODER_H264DECODER_H_
#define THIRD_PARTY_H264_WINRT_H264DECODER_H264DECODER_H_

#include <mfapi.h>
#include <mfidl.h>
#include <Mfreadwrite.h>
#include <mferror.h>
#include <wrl.h>
#include "Utils/SampleAttributeQueue.h"
#include "webrtc/video_decoder.h"
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"

#pragma comment(lib, "mfreadwrite")
#pragma comment(lib, "mfplat")
#pragma comment(lib, "mfuuid")

using Microsoft::WRL::ComPtr;

namespace webrtc {

class H264WinRTDecoderImpl : public VideoDecoder {
 public:
  H264WinRTDecoderImpl();

  virtual ~H264WinRTDecoderImpl();

  int InitDecode(const VideoCodec* inst, int number_of_cores) override;

  int Decode(const EncodedImage& input_image,
    bool missing_frames,
    const RTPFragmentationHeader* fragmentation,
    const CodecSpecificInfo* codec_specific_info,
    int64_t /*render_time_ms*/) override;

  int RegisterDecodeCompleteCallback(DecodedImageCallback* callback) override;

  int Release() override;

  const char* ImplementationName() const override;

 private:
  void UpdateVideoFrameDimensions(const EncodedImage& input_image);

 private:
  uint32_t width_;
  uint32_t height_;
  std::unique_ptr<webrtc::CriticalSectionWrapper> _cbLock;
  DecodedImageCallback* decodeCompleteCallback_;
};  // end of H264WinRTDecoderImpl class

}  // namespace webrtc

#endif  // THIRD_PARTY_H264_WINRT_H264DECODER_H264DECODER_H_
