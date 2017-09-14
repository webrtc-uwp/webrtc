/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_CAPTURE_RESULT_DESKTOP_CAPTURER_WRAPPER_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_CAPTURE_RESULT_DESKTOP_CAPTURER_WRAPPER_H_

#include <memory>

#include "webrtc/modules/desktop_capture/desktop_capturer.h"
#include "webrtc/modules/desktop_capture/desktop_capturer_wrapper.h"

namespace webrtc {

// A DesktopCapturerWrapper implementation to capture the result of
// |base_capturer|. Derived classes are expected to override
// OnCaptureResult() function to observe the DesktopFrame returned by
// |base_capturer|.
class CaptureResultDesktopCapturerWrapper
    : public DesktopCapturerWrapper,
      public DesktopCapturer::Callback {
 public:
  using Callback = DesktopCapturer::Callback;

  explicit CaptureResultDesktopCapturerWrapper(
      std::unique_ptr<DesktopCapturer> base_capturer);
  ~CaptureResultDesktopCapturerWrapper() override;

  // DesktopCapturer implementations.
  void Start(Callback* callback) final;

 protected:
  // Derived classes are expected to use this function to deliver the capture
  // result to |callback_|. It does some very basic sanity checks to the
  // parameters, e.g. it always sets |result| to ERROR_TEMPORARY if |frame| is
  // empty or drops |frame| if |result| is not SUCCESS.
  // Note, this function triggers assertion failure if |callback_| is not
  // correctly set.
  void PublishCaptureResult(Result result, std::unique_ptr<DesktopFrame> frame);

 private:
  Callback* callback_ = nullptr;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_DESKTOP_CAPTURE_CAPTURE_RESULT_DESKTOP_CAPTURER_WRAPPER_H_
