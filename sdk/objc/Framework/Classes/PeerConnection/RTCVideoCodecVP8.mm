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
#import "WebRTC/RTCVideoCodecVP8.h"

#pragma mark - Encoder

@implementation RTCVideoEncoderVP8

- (instancetype)init {
  return [super initWithVideoCodec:cricket::VideoCodec("VP8")];
}

@end

#pragma mark - Decoder

@implementation RTCVideoDecoderVP8

- (instancetype)init {
  return [super initWithVideoCodec:webrtc::kVideoCodecVP8];
}

@end
