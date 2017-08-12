/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/window_under_point.h"

#include "webrtc/modules/desktop_capture/x11/window_list_utils.h"

namespace webrtc {

WindowId GetWindowUnderPoint(XAtomCache* cache, DesktopVector point) {
  WindowId id;
  if (!(GetWindowList(cache,
                      [&id, cache, point](::Window window) {
                        DesktopRect rect;
                        if (GetWindowRect(cache->display(), window, &rect) &&
                            rect.Contains(point)) {
                          id = window;
                          return false;
                        }
                        return true;
                      }))) {
    return kNullWindowId;
  }
  return id;
}

}  // namespace webrtc
