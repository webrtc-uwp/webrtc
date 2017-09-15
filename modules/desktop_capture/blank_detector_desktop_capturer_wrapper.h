/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_BLANK_DETECTOR_DESKTOP_CAPTURER_WRAPPER_H_
#define MODULES_DESKTOP_CAPTURE_BLANK_DETECTOR_DESKTOP_CAPTURER_WRAPPER_H_

#include <memory>

#include "modules/desktop_capture/capture_result_desktop_capturer_wrapper.h"
#include "modules/desktop_capture/rgba_color.h"

namespace webrtc {

// A DesktopCapturer wrapper detects the return value of its owned
// DesktopCapturer implementation. If sampled pixels returned by the
// DesktopCapturer implementation all equal to the blank pixel, this wrapper
// returns ERROR_TEMPORARY. If the DesktopCapturer implementation fails for too
// many times, this wrapper returns ERROR_PERMANENT.
class BlankDetectorDesktopCapturerWrapper final
    : public CaptureResultDesktopCapturerWrapper,
      public CaptureResultDesktopCapturerWrapper::ResultObserver {
 public:
  // Creates BlankDetectorDesktopCapturerWrapper. BlankDesktopCapturerWrapper
  // takes ownership of |capturer|. The |blank_pixel| is the unmodified color
  // returned by the |capturer|.
  BlankDetectorDesktopCapturerWrapper(std::unique_ptr<DesktopCapturer> capturer,
                                      RgbaColor blank_pixel);
  ~BlankDetectorDesktopCapturerWrapper() override;

 private:
  // CaptureResultDesktopCapturer::ResultObserver implementations.
  void Observe(Result* result, std::unique_ptr<DesktopFrame>* frame) override;

  bool IsBlankFrame(const DesktopFrame& frame) const;

  // Detects whether pixel at (x, y) equals to |blank_pixel_|.
  bool IsBlankPixel(const DesktopFrame& frame, int x, int y) const;

  const RgbaColor blank_pixel_;

  // Whether a non-blank frame has been received.
  bool non_blank_frame_received_ = false;

  // Whether the last frame is blank.
  bool last_frame_is_blank_ = false;

  // Whether current frame is the first frame.
  bool is_first_frame_ = true;
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_BLANK_DETECTOR_DESKTOP_CAPTURER_WRAPPER_H_
