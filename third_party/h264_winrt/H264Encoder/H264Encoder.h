/*
*  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
*
*  Use of this source code is governed by a BSD-style license
*  that can be found in the LICENSE file in the root of the source
*  tree. An additional intellectual property rights grant can be found
*  in the file PATENTS.  All contributing project authors may
*  be found in the AUTHORS file in the root of the source tree.
*/

#ifndef THIRD_PARTY_H264_WINRT_H264ENCODER_H264ENCODER_H_
#define THIRD_PARTY_H264_WINRT_H264ENCODER_H264ENCODER_H_

#include <mfapi.h>
#include <mfidl.h>
#include <Mfreadwrite.h>
#include <mferror.h>
#include <vector>
#include "H264MediaSink.h"
#include "IH264EncodingCallback.h"
#include "Utils/SampleAttributeQueue.h"
#include "webrtc/video_encoder.h"
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"
#include "webrtc/modules/video_coding/utility/quality_scaler.h"
#include "webrtc/modules/rtp_rtcp/source/h264_bitstream_parser.h"
#include "webrtc/system_wrappers/include/tick_util.h"

#pragma comment(lib, "mfreadwrite")
#pragma comment(lib, "mfplat")
#pragma comment(lib, "mfuuid")

namespace webrtc {

class H264MediaSink;

class H264WinRTEncoderImpl : public VideoEncoder, public IH264EncodingCallback {
 public:
  H264WinRTEncoderImpl();

  ~H264WinRTEncoderImpl();

  // === VideoEncoder overrides ===
  int InitEncode(const VideoCodec* codec_settings,
    int number_of_cores, size_t max_payload_size) override;
  int RegisterEncodeCompleteCallback(EncodedImageCallback* callback) override;
  int Release() override;
  int Encode(const VideoFrame& input_image,
    const CodecSpecificInfo* codec_specific_info,
    const std::vector<FrameType>* frame_types) override;
  int SetChannelParameters(uint32_t packet_loss, int64_t rtt) override;
  int SetRates(uint32_t new_bitrate_kbit, uint32_t frame_rate) override;
  void OnDroppedFrame(uint32_t timestamp) override;
  const char* ImplementationName() const override;

  // === IH264EncodingCallback overrides ===
  void OnH264Encoded(ComPtr<IMFSample> sample) override;

 private:
  ComPtr<IMFSample> FromVideoFrame(const VideoFrame& frame);
  int InitEncoderWithSettings(const VideoCodec* inst);

 private:
  std::unique_ptr<webrtc::CriticalSectionWrapper> _lock;
  std::unique_ptr<webrtc::CriticalSectionWrapper> _callbackLock;
  bool inited_;
  const CodecSpecificInfo* codecSpecificInfo_;
  ComPtr<IMFSinkWriter> sinkWriter_;
  ComPtr<IMFAttributes> sinkWriterCreationAttributes_;
  ComPtr<IMFAttributes> sinkWriterEncoderAttributes_;
  ComPtr<H264MediaSink> mediaSink_;
  EncodedImageCallback* encodedCompleteCallback_;
  DWORD streamIndex_;
  LONGLONG startTime_;
  LONGLONG lastTimestampHns_;
  bool firstFrame_;
  int framePendingCount_;
  DWORD frameCount_;
  bool lastFrameDropped_;
  UINT32 currentWidth_;
  UINT32 currentHeight_;
  UINT32 currentBitrateBps_;
  UINT32 currentFps_;
  webrtc::TickTime lastTimeSettingsChanged_;

  struct CachedFrameAttributes {
    uint32_t timestamp;
    uint64_t ntpTime;
    uint64_t captureRenderTime;
    uint32_t frameWidth;
    uint32_t frameHeight;
  };
  SampleAttributeQueue<CachedFrameAttributes> _sampleAttributeQueue;

  // Used to dynamically scale down the frames
  // in response to QP and dropped frames stats.
  QualityScaler quality_scaler_;
  // Used to parse QP values out of the samples.
  H264BitstreamParser _h264Parser;
  webrtc::Scaler _scaler;
  // Caching the codec received in InitEncode().
  VideoCodec codec_;
};  // end of H264WinRTEncoderImpl class

}  // namespace webrtc
#endif  // THIRD_PARTY_H264_WINRT_H264ENCODER_H264ENCODER_H_

