/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_SCREEN_DRAWER_LOCK_POSIX_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_SCREEN_DRAWER_LOCK_POSIX_H_

#include <semaphore.h>

#include "webrtc/modules/desktop_capture/screen_drawer.h"

namespace webrtc {

class ScreenDrawerLockPosix final : public ScreenDrawerLock {
 public:
  ScreenDrawerLockPosix();
  ~ScreenDrawerLockPosix() override;

 private:
  sem_t* semaphore_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_DESKTOP_CAPTURE_SCREEN_DRAWER_LOCK_POSIX_H_
