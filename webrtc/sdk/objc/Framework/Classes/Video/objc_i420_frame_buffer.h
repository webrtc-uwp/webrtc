/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SDK_OBJC_FRAMEWORK_CLASSES_VIDEO_OBJC_I420_FRAME_BUFFER_H_
#define WEBRTC_SDK_OBJC_FRAMEWORK_CLASSES_VIDEO_OBJC_I420_FRAME_BUFFER_H_

#include "WebRTC/RTCMacros.h"
#include "webrtc/common_video/include/video_frame_buffer.h"

RTC_FWD_DECL_OBJC_CLASS(NSObject);

namespace webrtc {

  class ObjCI420FrameBuffer : public I420BufferInterface {
  public:
    // `NSObject*` is the C++ friendly version of id<RTCI420Buffer>
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

}  // namespace webrtc

#endif  // WEBRTC_SDK_OBJC_FRAMEWORK_CLASSES_VIDEO_OBJC_I420_FRAME_BUFFER_H_
