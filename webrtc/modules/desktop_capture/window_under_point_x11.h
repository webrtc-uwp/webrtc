/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_WINDOW_UNDER_POINT_X11_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_WINDOW_UNDER_POINT_X11_H_

#include "webrtc/modules/desktop_capture/desktop_capture_types.h"
#include "webrtc/modules/desktop_capture/desktop_geometry.h"

namespace webrtc {

// This is the X11 version of GetWindowUnderPoint().
class XAtomCache;

WindowId GetWindowUnderPoint(XAtomCache* cache, DesktopVector point);

}  // namespace webrtc

#endif  // WEBRTC_MODULES_DESKTOP_CAPTURE_WINDOW_UNDER_POINT_X11_H_
