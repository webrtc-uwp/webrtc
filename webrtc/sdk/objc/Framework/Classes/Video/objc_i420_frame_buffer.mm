/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/sdk/objc/Framework/Classes/Video/objc_i420_frame_buffer.h"

#import "WebRTC/RTCVideoFrameBuffer.h"

#include "libyuv/scale.h"
#include "webrtc/api/video/i420_buffer.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"

namespace webrtc {

ObjCI420FrameBuffer::ObjCI420FrameBuffer(NSObject* frame_buffer)
  : frame_buffer_(frame_buffer),
    width_(((id<RTCI420Buffer>)frame_buffer).width),
    height_(((id<RTCI420Buffer>)frame_buffer).height) {}

ObjCI420FrameBuffer::~ObjCI420FrameBuffer() {}

int ObjCI420FrameBuffer::width() const {
  return width_;
}

int ObjCI420FrameBuffer::height() const {
  return height_;
}

const uint8_t* ObjCI420FrameBuffer::DataY() const {
  return ((id<RTCI420Buffer>)frame_buffer_).dataY;
}
const uint8_t* ObjCI420FrameBuffer::DataU() const {
  return ((id<RTCI420Buffer>)frame_buffer_).dataU;
}
const uint8_t* ObjCI420FrameBuffer::DataV() const {
  return ((id<RTCI420Buffer>)frame_buffer_).dataV;
}

int ObjCI420FrameBuffer::StrideY() const {
  return ((id<RTCI420Buffer>)frame_buffer_).strideY;
}
int ObjCI420FrameBuffer::StrideU() const {
  return ((id<RTCI420Buffer>)frame_buffer_).strideU;
}
int ObjCI420FrameBuffer::StrideV() const {
  return ((id<RTCI420Buffer>)frame_buffer_).strideV;
}

}  // namespace webrtc
