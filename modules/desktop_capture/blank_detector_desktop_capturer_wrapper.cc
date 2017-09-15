/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/blank_detector_desktop_capturer_wrapper.h"

#include <algorithm>
#include <utility>

#include "modules/desktop_capture/desktop_geometry.h"
#include "rtc_base/checks.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {

BlankDetectorDesktopCapturerWrapper::BlankDetectorDesktopCapturerWrapper(
    std::unique_ptr<DesktopCapturer> capturer,
    RgbaColor blank_pixel)
    : CaptureResultDesktopCapturerWrapper(std::move(capturer), this),
      blank_pixel_(blank_pixel) {}

BlankDetectorDesktopCapturerWrapper::~BlankDetectorDesktopCapturerWrapper() =
    default;

void BlankDetectorDesktopCapturerWrapper::Observe(
    Result* result,
    std::unique_ptr<DesktopFrame>* frame) {
  if (*result != Result::SUCCESS || non_blank_frame_received_) {
    return;
  }

  RTC_DCHECK(*frame);

  // If nothing has been changed in current frame, we do not need to check it
  // again.
  if (!(*frame)->updated_region().is_empty() || is_first_frame_) {
    last_frame_is_blank_ = IsBlankFrame(**frame);
    is_first_frame_ = false;
  }
  RTC_HISTOGRAM_BOOLEAN("WebRTC.DesktopCapture.BlankFrameDetected",
                        last_frame_is_blank_);
  if (!last_frame_is_blank_) {
    non_blank_frame_received_ = true;
    return;
  }

  *result = Result::ERROR_TEMPORARY;
  frame->reset();
}

bool BlankDetectorDesktopCapturerWrapper::IsBlankFrame(
    const DesktopFrame& frame) const {
  // We will check 7489 pixels for a frame with 1024 x 768 resolution.
  for (int i = 0; i < frame.size().width() * frame.size().height(); i += 105) {
    const int x = i % frame.size().width();
    const int y = i / frame.size().width();
    if (!IsBlankPixel(frame, x, y)) {
      return false;
    }
  }

  // We are verifying the pixel in the center as well.
  return IsBlankPixel(frame, frame.size().width() / 2,
                      frame.size().height() / 2);
}

bool BlankDetectorDesktopCapturerWrapper::IsBlankPixel(
    const DesktopFrame& frame,
    int x,
    int y) const {
  uint8_t* pixel_data = frame.GetFrameDataAtPos(DesktopVector(x, y));
  return RgbaColor(pixel_data) == blank_pixel_;
}

}  // namespace webrtc
