/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_ACTIVE_STATE_MONITOR_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_ACTIVE_STATE_MONITOR_H_

#include "webrtc/modules/desktop_capture/state_monitor.h"

namespace webrtc {

// Monitors the change and the latest value of |T|, which is provided by Get()
// function.
template <typename T>
class ActiveStateMonitor : public StateMonitor<T> {
 public:
  bool IsChanged() {
    return StateMonitor<T>::Set(Get());
  }

 protected:
  virtual T Get() = 0;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_DESKTOP_CAPTURE_ACTIVE_STATE_MONITOR_H_
