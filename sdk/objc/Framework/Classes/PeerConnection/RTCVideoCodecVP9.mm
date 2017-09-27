/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#import <Foundation/Foundation.h>

#import "RTCInternalSoftwareVideoDecoder+Private.h"
#import "RTCInternalSoftwareVideoEncoder+Private.h"
#import "WebRTC/RTCVideoDecoderVP9.h"
#import "WebRTC/RTCVideoEncoderVP9.h"

#include "modules/video_coding/codecs/vp9/include/vp9.h"

#pragma mark - Encoder

@implementation RTCVideoEncoderVP9

- (instancetype)init {
  return [super
      initWithNativeEncoder:std::unique_ptr<webrtc::VideoEncoder>(webrtc::VP9Encoder::Create())];
}

@end

#pragma mark - Decoder

@implementation RTCVideoDecoderVP9

- (instancetype)init {
  return [super
      initWithNativeDecoder:std::unique_ptr<webrtc::VideoDecoder>(webrtc::VP9Decoder::Create())];
}

@end
