/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_STATE_MONITOR_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_STATE_MONITOR_H_

namespace webrtc {

// Monitors the change and the latest value of |T|.
template <typename T>
class StateMonitor {
 public:
  virtual ~StateMonitor() = default;

  const T& last() const { return last_; }

  // Resets to the initial state.
  void Reset() { initialized_ = false; }

 protected:
  // Sets the |last_| to value. Returns true if a previous |last_| was recorded
  // and differs from |value|.
  bool Set(const T& value) {
    if (!initialized_) {
      initialized_ = true;
      last_ = value;
      return false;
    }

    // Chromium does not like operator overloading, using equals() function
    // instead.
    if (last_.equals(value)) {
      return false;
    }

    last_ = value;
    return true;
  }

 private:
  T last_;
  bool initialized_ = false;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_DESKTOP_CAPTURE_STATE_MONITOR_H_
