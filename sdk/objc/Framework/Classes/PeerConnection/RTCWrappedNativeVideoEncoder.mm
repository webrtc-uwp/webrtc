/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#import "NSString+StdString.h"
#import "RTCVideoCodec+Private.h"
#import "RTCVideoFrame+Private.h"
#import "RTCWrappedNativeVideoEncoder.h"

#include "rtc_base/timeutils.h"
#include "sdk/objc/Framework/Classes/Video/objc_frame_buffer.h"

const size_t kDefaultPayloadSize = 1440;

class WrappedEncodedImageCallback : public webrtc::EncodedImageCallback {
 public:
  Result OnEncodedImage(const webrtc::EncodedImage &encoded_image,
                        const webrtc::CodecSpecificInfo *codec_specific_info,
                        const webrtc::RTPFragmentationHeader *fragmentation) {
    RTCEncodedImage *image = [[RTCEncodedImage alloc] initWithNativeEncodedImage:encoded_image];
    id<RTCCodecSpecificInfo> info = nil;
    RTCRtpFragmentationHeader *header =
        [[RTCRtpFragmentationHeader alloc] initWithNativeFragmentationHeader:fragmentation];
    callback(image, info, header);
    return Result(Result::OK, 0);
  }

  RTCVideoEncoderCallback callback;
};

@implementation RTCWrappedNativeVideoEncoder {
  std::unique_ptr<webrtc::VideoEncoder> _wrappedEncoder;

  WrappedEncodedImageCallback *_encodedImageCallback;
}

- (instancetype)initWithNativeEncoder:(std::unique_ptr<webrtc::VideoEncoder>)encoder {
  if (self = [super init]) {
    _wrappedEncoder = std::move(encoder);
  }

  return self;
}

- (std::unique_ptr<webrtc::VideoEncoder>)releaseWrappedEncoder {
  return std::move(_wrappedEncoder);
}

- (void)dealloc {
  if (_encodedImageCallback) {
    delete _encodedImageCallback;
  }
}

#pragma mark - RTCVideoEncoder

- (void)setCallback:(RTCVideoEncoderCallback)callback {
  _encodedImageCallback = new WrappedEncodedImageCallback();
  _encodedImageCallback->callback = callback;
  _wrappedEncoder->RegisterEncodeCompleteCallback(_encodedImageCallback);
}

- (NSInteger)startEncodeWithSettings:(RTCVideoEncoderSettings *)settings
                       numberOfCores:(int)numberOfCores {
  webrtc::VideoCodec codecSettings = [settings nativeVideoCodec];
  return _wrappedEncoder->InitEncode(&codecSettings, numberOfCores, kDefaultPayloadSize);
}

- (NSInteger)releaseEncoder {
  return _wrappedEncoder->Release();
}

- (NSInteger)encode:(RTCVideoFrame *)frame
    codecSpecificInfo:(id<RTCCodecSpecificInfo>)info
           frameTypes:(NSArray<NSNumber *> *)frameTypes {
  webrtc::VideoFrame videoFrame = [frame nativeVideoFrame];

  // Handle types that can be converted into one of webrtc::CodecSpecificInfo's hard coded cases.
  webrtc::CodecSpecificInfo codecSpecificInfo;
  if ([info isKindOfClass:[RTCCodecSpecificInfoH264 class]]) {
    codecSpecificInfo = [(RTCCodecSpecificInfoH264 *)info nativeCodecSpecificInfo];
  }

  std::vector<webrtc::FrameType> nativeFrameTypes;
  for (NSNumber *frameType in frameTypes) {
    RTCFrameType rtcFrameType = (RTCFrameType)frameType.unsignedIntegerValue;
    nativeFrameTypes.push_back((webrtc::FrameType)rtcFrameType);
  }

  return _wrappedEncoder->Encode(videoFrame, &codecSpecificInfo, &nativeFrameTypes);
}

- (int)setBitrate:(uint32_t)bitrateKbit framerate:(uint32_t)framerate {
  return _wrappedEncoder->SetRates(bitrateKbit, framerate);
}

- (NSString *)implementationName {
  return [NSString stringForStdString:_wrappedEncoder->ImplementationName()];
}

- (RTCVideoEncoderQpThresholds *)scalingSettings {
  rtc::Optional<webrtc::VideoEncoder::QpThresholds> thresholds =
      _wrappedEncoder->GetScalingSettings().thresholds;
  if (thresholds.has_value()) {
    return [[RTCVideoEncoderQpThresholds alloc] initWithThresholdsLow:thresholds->low
                                                                 high:thresholds->high];
  } else {
    return nil;
  }
}

@end
