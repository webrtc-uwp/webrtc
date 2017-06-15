/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "WebRTC/RTCVideoFrameBuffer.h"

#include "webrtc/base/checks.h"
#include "webrtc/system_wrappers/include/aligned_malloc.h"

// Aligning pointer to 64 bytes for improved performance, e.g. use SIMD.
static const int kBufferAlignment = 64;

@implementation RTCI420Buffer {
 @protected
  uint8_t *_data;
  int _width;
  int _height;
  int _strideY;
  int _strideU;
  int _strideV;
}

@synthesize width = _width;
@synthesize height = _height;

@synthesize strideY = _strideY;
@synthesize strideU = _strideU;
@synthesize strideV = _strideV;

- (instancetype)initWithWidth:(int)width height:(int)height {
  return [self initWithWidth:width
                      height:height
                     strideY:width
                     strideU:(width + 1) / 2
                     strideV:(width + 1) / 2];
}

- (instancetype)initWithWidth:(int)width
                       height:(int)height
                      strideY:(int)strideY
                      strideU:(int)strideU
                      strideV:(int)strideV {
  if (self = [super init]) {
    _height = height;
    _width = width;
    _strideY = strideY;
    _strideU = strideU;
    _strideV = strideV;

    int i420DataSize = strideY * height + (strideU + strideV) * ((height + 1) / 2);
    _data = static_cast<uint8_t *>(webrtc::AlignedMalloc(i420DataSize, kBufferAlignment));

    RTC_DCHECK_GT(width, 0);
    RTC_DCHECK_GT(height, 0);
    RTC_DCHECK_GE(strideY, width);
    RTC_DCHECK_GE(strideU, (width + 1) / 2);
    RTC_DCHECK_GE(strideV, (width + 1) / 2);
  }

  return self;
}

- (int)chromaWidth {
  return (_width + 1) / 2;
}

- (int)chromaHeight {
  return (_height + 1) / 2;
}

- (const uint8_t *)dataY {
  return _data;
}

- (const uint8_t *)dataU {
  return _data + _strideY * _height;
}

- (const uint8_t *)dataV {
  return _data + _strideY * _height + _strideU * ((_height + 1) / 2);
}

- (id<RTCI420Buffer>)toI420 {
  return self;
}

@end

@implementation RTCMutableI420Buffer

- (uint8_t *)mutableDataY {
  return _data;
}

- (uint8_t *)mutableDataU {
  return _data + _strideY * _height;
}

- (uint8_t *)mutableDataV {
  return _data + _strideY * _height + _strideU * ((_height + 1) / 2);
}

@end
