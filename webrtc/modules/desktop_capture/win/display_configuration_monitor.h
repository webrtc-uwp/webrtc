/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_WIN_DISPLAY_CONFIGURATION_MONITOR_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_WIN_DISPLAY_CONFIGURATION_MONITOR_H_

#include "webrtc/modules/desktop_capture/active_state_monitor.h"
#include "webrtc/modules/desktop_capture/desktop_geometry.h"

namespace webrtc {

// A passive monitor to detect the change of display configuration on a Windows
// system.
// TODO(zijiehe): Also check for pixel format changes.
class DisplayConfigurationMonitor final
    : public ActiveStateMonitor<DesktopRect> {
 protected:
  DesktopRect Get() override;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_DESKTOP_CAPTURE_WIN_DISPLAY_CONFIGURATION_MONITOR_H_
