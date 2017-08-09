/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/screen_drawer_lock_posix.h"

#include <fcntl.h>
#include <sys/stat.h>

namespace webrtc {

namespace {

// A uuid as the name of semaphore.
static constexpr char kSemaphoreName[] =
    "/global-screen-drawer-linux-54fe5552-8047-11e6-a725-3f429a5b4fb4";

}  // namespace

ScreenDrawerLockPosix::ScreenDrawerLockPosix() {
  semaphore_ =
      sem_open(kSemaphoreName, O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO, 1);
  sem_wait(semaphore_);
}

ScreenDrawerLockPosix::~ScreenDrawerLockPosix() {
  sem_post(semaphore_);
  sem_close(semaphore_);
  // sem_unlink a named semaphore won't wait until other clients to release the
  // sem_t. So if a new process starts, it will sem_open a different kernel
  // object and eventually breaks the cross-process lock.
  // TODO(zijiehe): Find the right time to sem_unlink the semaphore.
  // sem_unlink(kSemaphoreName);
}

}  // namespace webrtc
