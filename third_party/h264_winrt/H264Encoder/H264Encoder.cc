/*
*  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
*
*  Use of this source code is governed by a BSD-style license
*  that can be found in the LICENSE file in the root of the source
*  tree. An additional intellectual property rights grant can be found
*  in the file PATENTS.  All contributing project authors may
*  be found in the AUTHORS file in the root of the source tree.
*/

#include "third_party/h264_winrt/H264Encoder/H264Encoder.h"

#include <Windows.h>
#include <stdlib.h>
#include <ppltasks.h>
#include <mfapi.h>
#include <robuffer.h>
#include <wrl.h>
#include <mfidl.h>
#include <codecapi.h>
#include <mfreadwrite.h>
#include <wrl\implements.h>
#include <sstream>
#include <vector>
#include <iomanip>

#include "H264StreamSink.h"
#include "H264MediaSink.h"
#include "Utils/Utils.h"
#include "webrtc/modules/video_coding/include/video_codec_interface.h"
#include "webrtc/system_wrappers/include/tick_util.h"
#include "webrtc/common_video/libyuv/include/scaler.h"
#include "libyuv/convert.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/win32.h"


#pragma comment(lib, "mfreadwrite")
#pragma comment(lib, "mfplat")
#pragma comment(lib, "mfuuid.lib")

namespace webrtc {

//////////////////////////////////////////
// H264 WinRT Encoder Implementation
//////////////////////////////////////////

H264WinRTEncoderImpl::H264WinRTEncoderImpl()
  : _lock(webrtc::CriticalSectionWrapper::CreateCriticalSection())
  , _callbackLock(webrtc::CriticalSectionWrapper::CreateCriticalSection())
  , firstFrame_(true)
  , startTime_(0)
  , framePendingCount_(0)
  , frameCount_(0)
  , lastFrameDropped_(false)
  , currentWidth_(0)
  , currentHeight_(0)
  , currentBitrateBps_(0)
  , currentFps_(0)
  , lastTimestampHns_(0) {
}

H264WinRTEncoderImpl::~H264WinRTEncoderImpl() {
  Release();
}

int H264WinRTEncoderImpl::InitEncode(const VideoCodec* inst,
  int /*number_of_cores*/,
  size_t /*maxPayloadSize */) {
  currentWidth_ = inst->width;
  currentHeight_ = inst->height;
  currentBitrateBps_ = inst->targetBitrate > 0 ? inst->targetBitrate * 1024 : currentWidth_ * currentHeight_ * 2.0;
  currentFps_ = inst->maxFramerate;
  quality_scaler_.Init(inst->qpMax / 2, 64, false, 0, currentWidth_, currentHeight_);
  return InitEncoderWithSettings(inst);
}

int H264WinRTEncoderImpl::InitEncoderWithSettings(const VideoCodec* inst) {
  HRESULT hr = S_OK;

  webrtc::CriticalSectionScoped csLock(_lock.get());

  ON_SUCCEEDED(MFStartup(MF_VERSION));

  // output media type (h264)
  ComPtr<IMFMediaType> mediaTypeOut;
  ON_SUCCEEDED(MFCreateMediaType(&mediaTypeOut));
  ON_SUCCEEDED(mediaTypeOut->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));
  ON_SUCCEEDED(mediaTypeOut->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264));
  // Lumia 635 and Lumia 1520 Windows phones don't work well
  // with constrained baseline profile.
  //ON_SUCCEEDED(mediaTypeOut->SetUINT32(MF_MT_MPEG2_PROFILE, eAVEncH264VProfile_ConstrainedBase));

  // Weight*Height*2 kbit represents a good balance between video quality and
  // the bandwidth that a 620 Windows phone can handle.
  ON_SUCCEEDED(mediaTypeOut->SetUINT32(
    MF_MT_AVG_BITRATE, currentBitrateBps_));
  ON_SUCCEEDED(mediaTypeOut->SetUINT32(
    MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive));
  ON_SUCCEEDED(MFSetAttributeSize(mediaTypeOut.Get(),
    MF_MT_FRAME_SIZE, currentWidth_, currentHeight_));
  ON_SUCCEEDED(MFSetAttributeRatio(mediaTypeOut.Get(),
    MF_MT_FRAME_RATE, currentFps_, 1));

  // input media type (nv12)
  ComPtr<IMFMediaType> mediaTypeIn;
  ON_SUCCEEDED(MFCreateMediaType(&mediaTypeIn));
  ON_SUCCEEDED(mediaTypeIn->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));
  ON_SUCCEEDED(mediaTypeIn->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12));
  ON_SUCCEEDED(mediaTypeIn->SetUINT32(
    MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive));
  ON_SUCCEEDED(mediaTypeIn->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE));
  ON_SUCCEEDED(MFSetAttributeSize(mediaTypeIn.Get(),
    MF_MT_FRAME_SIZE, currentWidth_, currentHeight_));
  ON_SUCCEEDED(MFSetAttributeRatio(mediaTypeIn.Get(),
    MF_MT_FRAME_RATE, currentFps_, 1));

  quality_scaler_.ReportFramerate(currentFps_);
  _scaler.Set(
    inst->width, inst->height,
    currentWidth_, currentHeight_,
    kI420, kI420,
    kScalePoint);

  // Create the media sink
  ON_SUCCEEDED(Microsoft::WRL::MakeAndInitialize<H264MediaSink>(&mediaSink_));

  // SinkWriter creation attributes
  ON_SUCCEEDED(MFCreateAttributes(&sinkWriterCreationAttributes_, 1));
  ON_SUCCEEDED(sinkWriterCreationAttributes_->SetUINT32(
    MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE));
  ON_SUCCEEDED(sinkWriterCreationAttributes_->SetUINT32(
    MF_SINK_WRITER_DISABLE_THROTTLING, TRUE));
  ON_SUCCEEDED(sinkWriterCreationAttributes_->SetUINT32(
    MF_LOW_LATENCY, TRUE));

  // Create the sink writer
  ON_SUCCEEDED(MFCreateSinkWriterFromMediaSink(mediaSink_.Get(),
    sinkWriterCreationAttributes_.Get(), &sinkWriter_));

  // Add the h264 output stream to the writer
  ON_SUCCEEDED(sinkWriter_->AddStream(mediaTypeOut.Get(), &streamIndex_));

  // SinkWriter encoder properties
  ON_SUCCEEDED(MFCreateAttributes(&sinkWriterEncoderAttributes_, 1));
  ON_SUCCEEDED(sinkWriter_->SetInputMediaType(streamIndex_, mediaTypeIn.Get(), nullptr));

  // Register this as the callback for encoded samples.
  ON_SUCCEEDED(mediaSink_->RegisterEncodingCallback(this));

  ON_SUCCEEDED(sinkWriter_->BeginWriting());

  codec_ = *inst;

  if (SUCCEEDED(hr)) {
    inited_ = true;
    lastTimeSettingsChanged_ = webrtc::TickTime::Now();
    return WEBRTC_VIDEO_CODEC_OK;
  } else {
    return hr;
  }
}

int H264WinRTEncoderImpl::RegisterEncodeCompleteCallback(
  EncodedImageCallback* callback) {
  webrtc::CriticalSectionScoped csLock(_callbackLock.get());
  encodedCompleteCallback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int H264WinRTEncoderImpl::Release() {
  // Use a temporary sink variable to prevent lock inversion
  // between the shutdown call and OnH264Encoded() callback.
  ComPtr<H264MediaSink> tmpMediaSink;

  {
    webrtc::CriticalSectionScoped csLock(_lock.get());
    sinkWriter_.Reset();
    if (mediaSink_ != nullptr) {
      tmpMediaSink = mediaSink_;
    }
    sinkWriterCreationAttributes_.Reset();
    sinkWriterEncoderAttributes_.Reset();
    mediaSink_.Reset();
    startTime_ = 0;
    lastTimestampHns_ = 0;
    firstFrame_ = true;
    inited_ = false;
    framePendingCount_ = 0;
    _sampleAttributeQueue.clear();
    webrtc::CriticalSectionScoped csCbLock(_callbackLock.get());
    encodedCompleteCallback_ = nullptr;
  }

  if (tmpMediaSink != nullptr) {
    tmpMediaSink->Shutdown();
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

#define DYNAMIC_SCALING

ComPtr<IMFSample> H264WinRTEncoderImpl::FromVideoFrame(const VideoFrame& frame) {
  HRESULT hr = S_OK;
  ComPtr<IMFSample> sample;
  ON_SUCCEEDED(MFCreateSample(sample.GetAddressOf()));

  ComPtr<IMFAttributes> sampleAttributes;
  ON_SUCCEEDED(sample.As(&sampleAttributes));

  quality_scaler_.OnEncodeFrame(frame);
#ifdef DYNAMIC_SCALING
  const VideoFrame& dstFrame = quality_scaler_.GetScaledFrame(frame, 16);
#else
  const VideoFrame& dstFrame = frame;
#endif

  if (SUCCEEDED(hr)) {
    auto totalSize = dstFrame.allocated_size(PlaneType::kYPlane) +
      dstFrame.allocated_size(PlaneType::kUPlane) +
      dstFrame.allocated_size(PlaneType::kVPlane);

    ComPtr<IMFMediaBuffer> mediaBuffer;
    ON_SUCCEEDED(MFCreateMemoryBuffer(totalSize, mediaBuffer.GetAddressOf()));

    BYTE* destBuffer = nullptr;
    if (SUCCEEDED(hr)) {
      DWORD cbMaxLength;
      DWORD cbCurrentLength;
      ON_SUCCEEDED(mediaBuffer->Lock(
        &destBuffer, &cbMaxLength, &cbCurrentLength));
    }

    if (SUCCEEDED(hr)) {
      BYTE* destUV = destBuffer +
        (dstFrame.stride(PlaneType::kYPlane) * dstFrame.height());
      libyuv::I420ToNV12(
        dstFrame.buffer(PlaneType::kYPlane), dstFrame.stride(PlaneType::kYPlane),
        dstFrame.buffer(PlaneType::kUPlane), dstFrame.stride(PlaneType::kUPlane),
        dstFrame.buffer(PlaneType::kVPlane), dstFrame.stride(PlaneType::kVPlane),
        destBuffer, dstFrame.stride(PlaneType::kYPlane),
        destUV, dstFrame.stride(PlaneType::kYPlane),
        dstFrame.width(),
        dstFrame.height());
    }

    {
      if (dstFrame.width() != (int)currentWidth_ || dstFrame.height() != (int)currentHeight_) {
        EncodedImageCallback* tempCallback = encodedCompleteCallback_;
        Release();
        {
          webrtc::CriticalSectionScoped csLock(_callbackLock.get());
          encodedCompleteCallback_ = tempCallback;
        }

        currentWidth_ = dstFrame.width();
        currentHeight_ = dstFrame.height();
        InitEncoderWithSettings(&codec_);
        LOG(LS_WARNING) << "Resolution changed to: " << dstFrame.width() << "x" << dstFrame.height();
      }
    }

    if (firstFrame_) {
      firstFrame_ = false;
      startTime_ = dstFrame.timestamp();
    }

    auto timestampHns = ((dstFrame.timestamp() - startTime_) / 90) * 1000 * 10;
    ON_SUCCEEDED(sample->SetSampleTime(timestampHns));

    if (SUCCEEDED(hr)) {
      auto durationHns = timestampHns - lastTimestampHns_;
      hr = sample->SetSampleDuration(durationHns);
    }

    if (SUCCEEDED(hr)) {
      lastTimestampHns_ = timestampHns;

      // Cache the frame attributes to get them back after the encoding.
      CachedFrameAttributes frameAttributes;
      frameAttributes.timestamp = dstFrame.timestamp();
      frameAttributes.ntpTime = dstFrame.ntp_time_ms();
      frameAttributes.captureRenderTime = dstFrame.render_time_ms();
      frameAttributes.frameWidth = dstFrame.width();
      frameAttributes.frameHeight = dstFrame.height();
      _sampleAttributeQueue.push(timestampHns, frameAttributes);
    }

    ON_SUCCEEDED(mediaBuffer->SetCurrentLength(
      dstFrame.width() * dstFrame.height() * 3 / 2));

    if (destBuffer != nullptr) {
      mediaBuffer->Unlock();
    }

    ON_SUCCEEDED(sample->AddBuffer(mediaBuffer.Get()));

    if (lastFrameDropped_) {
      lastFrameDropped_ = false;
      sampleAttributes->SetUINT32(MFSampleExtension_Discontinuity, TRUE);
    }
  }

  return sample;
}

int H264WinRTEncoderImpl::Encode(
  const VideoFrame& frame,
  const CodecSpecificInfo* codec_specific_info,
  const std::vector<FrameType>* frame_types) {
  {
    webrtc::CriticalSectionScoped csLock(_lock.get());
    if (!inited_) {
      return -1;
    }
  }


  if (frame_types != nullptr) {
    for (auto frameType : *frame_types) {
      if (frameType == kVideoFrameKey) {
        LOG(LS_INFO) << "Key frame requested in H264 encoder.";
        ComPtr<IMFSinkWriterEncoderConfig> encoderConfig;
        sinkWriter_.As(&encoderConfig);
        ComPtr<IMFAttributes> encoderAttributes;
        MFCreateAttributes(&encoderAttributes, 1);
        encoderAttributes->SetUINT32(CODECAPI_AVEncVideoForceKeyFrame, TRUE);
        encoderConfig->PlaceEncodingParameters(streamIndex_, encoderAttributes.Get());
        break;
      }
    }
  }

  HRESULT hr = S_OK;

  codecSpecificInfo_ = codec_specific_info;

  ComPtr<IMFSample> sample;
  {
    webrtc::CriticalSectionScoped csLock(_lock.get());
    if (_sampleAttributeQueue.size() > 2) {
      quality_scaler_.ReportDroppedFrame();
      return WEBRTC_VIDEO_CODEC_OK;
    }
    sample = FromVideoFrame(frame);
  }

  ON_SUCCEEDED(sinkWriter_->WriteSample(streamIndex_, sample.Get()));

  webrtc::CriticalSectionScoped csLock(_lock.get());
  // Some threads online mention this is useful to do regularly.
  ++frameCount_;
  if (frameCount_ % 30 == 0) {
    ON_SUCCEEDED(sinkWriter_->NotifyEndOfSegment(streamIndex_));
  }

  ++framePendingCount_;
  return WEBRTC_VIDEO_CODEC_OK;
}

void H264WinRTEncoderImpl::OnH264Encoded(ComPtr<IMFSample> sample) {
  DWORD totalLength;
  HRESULT hr = S_OK;
  ON_SUCCEEDED(sample->GetTotalLength(&totalLength));

  ComPtr<IMFMediaBuffer> buffer;
  hr = sample->GetBufferByIndex(0, &buffer);

  if (SUCCEEDED(hr)) {
    BYTE* byteBuffer;
    DWORD maxLength;
    DWORD curLength;
    hr = buffer->Lock(&byteBuffer, &maxLength, &curLength);
    if (FAILED(hr)) {
      return;
    }
    if (curLength == 0) {
      LOG(LS_WARNING) << "Got empty sample.";
      buffer->Unlock();
      return;
    }
    std::vector<byte> sendBuffer;
    sendBuffer.resize(curLength);
    memcpy(sendBuffer.data(), byteBuffer, curLength);
    hr = buffer->Unlock();
    if (FAILED(hr)) {
      return;
    }

    // sendBuffer is not copied here.
    EncodedImage encodedImage(sendBuffer.data(), curLength, curLength);

    ComPtr<IMFAttributes> sampleAttributes;
    hr = sample.As(&sampleAttributes);
    if (SUCCEEDED(hr)) {
      UINT32 cleanPoint;
      hr = sampleAttributes->GetUINT32(
        MFSampleExtension_CleanPoint, &cleanPoint);
      if (SUCCEEDED(hr) && cleanPoint) {
        encodedImage._completeFrame = true;
        encodedImage._frameType = kVideoFrameKey;
      }
    }

    // Scan for and create mark all fragments.
    RTPFragmentationHeader fragmentationHeader;
    uint32_t fragIdx = 0;
    for (uint32_t i = 0; i < sendBuffer.size() - 5; ++i) {
      byte* ptr = sendBuffer.data() + i;
      int prefixLengthFound = 0;
      if (ptr[0] == 0x00 && ptr[1] == 0x00 && ptr[2] == 0x00 && ptr[3] == 0x01
        && ((ptr[4] & 0x1f) != 0x09 /* ignore access unit delimiters */)) {
        prefixLengthFound = 4;
      } else if (ptr[0] == 0x00 && ptr[1] == 0x00 && ptr[2] == 0x01
        && ((ptr[3] & 0x1f) != 0x09 /* ignore access unit delimiters */)) {
        prefixLengthFound = 3;
      }

      // Found a key frame, mark is as such in case
      // MFSampleExtension_CleanPoint wasn't set on the sample.
      if (prefixLengthFound > 0 && (ptr[prefixLengthFound] & 0x1f) == 0x05) {
        encodedImage._completeFrame = true;
        encodedImage._frameType = kVideoFrameKey;
      }

      if (prefixLengthFound > 0) {
        fragmentationHeader.VerifyAndAllocateFragmentationHeader(fragIdx + 1);
        fragmentationHeader.fragmentationOffset[fragIdx] = i + prefixLengthFound;
        fragmentationHeader.fragmentationLength[fragIdx] = 0;  // We'll set that later
        // Set the length of the previous fragment.
        if (fragIdx > 0) {
          fragmentationHeader.fragmentationLength[fragIdx - 1] =
            i - fragmentationHeader.fragmentationOffset[fragIdx - 1];
        }
        fragmentationHeader.fragmentationPlType[fragIdx] = 0;
        fragmentationHeader.fragmentationTimeDiff[fragIdx] = 0;
        ++fragIdx;
        i += 5;
      }
    }
    // Set the length of the last fragment.
    if (fragIdx > 0) {
      fragmentationHeader.fragmentationLength[fragIdx - 1] =
        sendBuffer.size() -
        fragmentationHeader.fragmentationOffset[fragIdx - 1];
    }

    {
      webrtc::CriticalSectionScoped csLock(_callbackLock.get());
      --framePendingCount_;
      if (encodedCompleteCallback_ == nullptr) {
        return;
      }


      _h264Parser.ParseBitstream(sendBuffer.data(), sendBuffer.size());
      int lastQp;
      if (_h264Parser.GetLastSliceQp(&lastQp)) {
        quality_scaler_.ReportQP(lastQp);
      }

      LONGLONG sampleTimestamp;
      sample->GetSampleTime(&sampleTimestamp);

      CachedFrameAttributes frameAttributes;
      if (_sampleAttributeQueue.pop(sampleTimestamp, frameAttributes)) {
        encodedImage._timeStamp = frameAttributes.timestamp;
        encodedImage.ntp_time_ms_ = frameAttributes.ntpTime;
        encodedImage.capture_time_ms_ = frameAttributes.captureRenderTime;
        encodedImage._encodedWidth = frameAttributes.frameWidth;
        encodedImage._encodedHeight = frameAttributes.frameHeight;
        encodedImage.adapt_reason_.quality_resolution_downscales =
          quality_scaler_.downscale_shift();
      }
      else {
        // No point in confusing the callback with a frame that doesn't
        // have correct attributes.
        return;
      }

      if (encodedCompleteCallback_ != nullptr) {
        encodedCompleteCallback_->Encoded(
          encodedImage, codecSpecificInfo_, &fragmentationHeader);
      }
    }
  }
}

int H264WinRTEncoderImpl::SetChannelParameters(
  uint32_t packetLoss, int64_t rtt) {
  return WEBRTC_VIDEO_CODEC_OK;
}

#define DYNAMIC_FPS
#define DYNAMIC_BITRATE

int H264WinRTEncoderImpl::SetRates(
  uint32_t new_bitrate_kbit, uint32_t new_framerate) {
  LOG(LS_INFO) << "H264WinRTEncoderImpl::SetRates("
    << new_bitrate_kbit << "kbit " << new_framerate << "fps)";

  // This may happen.  Ignore it.
  if (new_framerate == 0) {
    return WEBRTC_VIDEO_CODEC_OK;
  }

  webrtc::CriticalSectionScoped csLock(_lock.get());
  if (sinkWriter_ == nullptr) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }

  bool bitrateUpdated = false;
  bool fpsUpdated = false;

#ifdef DYNAMIC_BITRATE
  if (currentBitrateBps_ != (new_bitrate_kbit * 1024)) {
    currentBitrateBps_ = new_bitrate_kbit * 1024;
    bitrateUpdated = true;
  }
#endif

#ifdef DYNAMIC_FPS
  // Fps changes seems to be expensive, make it granular to several frames per second.
  if (currentFps_ != new_framerate && std::abs((int)currentFps_ - (int)new_framerate) > 5) {
    currentFps_ = new_framerate;
    fpsUpdated = true;
  }
#endif
  quality_scaler_.ReportFramerate(new_framerate);

  if (bitrateUpdated || fpsUpdated) {
    if ((webrtc::TickTime::Now() - lastTimeSettingsChanged_).Milliseconds() < 15000) {
      LOG(LS_INFO) << "Last time settings changed was too soon, skipping this SetRates().\n";
      return WEBRTC_VIDEO_CODEC_OK;
    }

    EncodedImageCallback* tempCallback = encodedCompleteCallback_;
    Release();
    {
      webrtc::CriticalSectionScoped csLock(_callbackLock.get());
      encodedCompleteCallback_ = tempCallback;
    }
    InitEncoderWithSettings(&codec_);
  }

  return WEBRTC_VIDEO_CODEC_OK;
}

void H264WinRTEncoderImpl::OnDroppedFrame(uint32_t timestamp) {
  LONGLONG timestampHns = 0;
  ComPtr<IMFSinkWriter> tempSinkWriter;
  {
    webrtc::CriticalSectionScoped csLock(_lock.get());
    quality_scaler_.ReportDroppedFrame();
    timestampHns = ((timestamp - startTime_) / 90) * 1000 * 10;
    lastFrameDropped_ = true;
    tempSinkWriter = sinkWriter_;
  }
  if (tempSinkWriter != nullptr) {
    sinkWriter_->SendStreamTick(streamIndex_, timestampHns);
  }
}

const char* H264WinRTEncoderImpl::ImplementationName() const {
  return "H264_MediaFoundation";
}

}  // namespace webrtc
