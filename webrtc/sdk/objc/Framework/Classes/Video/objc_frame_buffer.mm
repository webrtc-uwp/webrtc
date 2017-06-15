/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/sdk/objc/Framework/Classes/Video/objc_frame_buffer.h"
#include "webrtc/sdk/objc/Framework/Classes/Video/objc_i420_frame_buffer.h"

#import "WebRTC/RTCVideoFrameBuffer.h"

#include "libyuv/scale.h"
#include "webrtc/api/video/i420_buffer.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"

namespace webrtc {

ObjCFrameBuffer::ObjCFrameBuffer(NSObject* frame_buffer)
    : frame_buffer_(frame_buffer),
      width_(((id<RTCVideoFrameBuffer>)frame_buffer).width),
      height_(((id<RTCVideoFrameBuffer>)frame_buffer).height) {}

ObjCFrameBuffer::~ObjCFrameBuffer() {}

VideoFrameBuffer::Type ObjCFrameBuffer::type() const {
  return VideoFrameBuffer::Type::kNative;
}

int ObjCFrameBuffer::width() const {
  return width_;
}

int ObjCFrameBuffer::height() const {
  return height_;
}

rtc::scoped_refptr<I420BufferInterface> ObjCFrameBuffer::ToI420() {
  id<RTCVideoFrameBuffer> rtcFrameBuffer = (id<RTCVideoFrameBuffer>)frame_buffer_;
  rtc::scoped_refptr<I420BufferInterface> buffer =
      new rtc::RefCountedObject<ObjCI420FrameBuffer>([rtcFrameBuffer toI420]);

  return buffer;
}

NSObject* ObjCFrameBuffer::wrapped_frame_buffer() const {
  return frame_buffer_;
}

}  // namespace webrtc
