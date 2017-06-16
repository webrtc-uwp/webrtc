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

#import "WebRTC/RTCVideoFrameBuffer.h"

#include "libyuv/scale.h"
#include "webrtc/api/video/i420_buffer.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"

namespace webrtc {

namespace {
class ObjCI420FrameBuffer : public I420BufferInterface {
 public:
  explicit ObjCI420FrameBuffer(NSObject* frame_buffer);
  ~ObjCI420FrameBuffer() override;

  int width() const override;
  int height() const override;

  const uint8_t* DataY() const override;
  const uint8_t* DataU() const override;
  const uint8_t* DataV() const override;

  int StrideY() const override;
  int StrideU() const override;
  int StrideV() const override;

 private:
  NSObject* frame_buffer_;
  int width_;
  int height_;
};
}

ObjCFrameBuffer::ObjCFrameBuffer(NSObject* frame_buffer)
    : frame_buffer_(frame_buffer),
      width_(((id<RTCVideoFrameBuffer>)frame_buffer).width),
      height_(((id<RTCVideoFrameBuffer>)frame_buffer).height) {}

ObjCFrameBuffer::~ObjCFrameBuffer() {}

VideoFrameBuffer::Type ObjCFrameBuffer::type() const {
  return Type::kNative;
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

/** ObjCFrameBuffer that conforms to I420BufferInterface by wrapping RTCI420Buffer */

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
