/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/screen_drawer.h"

#include <stdint.h>

#include "webrtc/rtc_base/logging.h"
#include "webrtc/rtc_base/random.h"
#include "webrtc/rtc_base/platform_thread.h"
#include "webrtc/rtc_base/timeutils.h"
#include "webrtc/system_wrappers/include/sleep.h"
#include "webrtc/test/gtest.h"

#if defined(WEBRTC_POSIX)
#include "semaphore.h"
#include "webrtc/modules/desktop_capture/screen_drawer_lock_posix.h"
#endif

namespace webrtc {

// These are a set of manual test cases, as we do not have an automatical way to
// detect whether a ScreenDrawer on a certain platform works well without
// ScreenCapturer(s). So you may execute these test cases with
// --gtest_also_run_disabled_tests --gtest_filter=ScreenDrawerTest.*.
TEST(ScreenDrawerTest, DISABLED_DrawRectangles) {
  std::unique_ptr<ScreenDrawer> drawer = ScreenDrawer::Create();
  if (!drawer) {
    LOG(LS_WARNING) << "No ScreenDrawer implementation for current platform.";
    return;
  }

  if (drawer->DrawableRegion().is_empty()) {
    LOG(LS_WARNING) << "ScreenDrawer of current platform does not provide a "
                       "non-empty DrawableRegion().";
    return;
  }

  DesktopRect rect = drawer->DrawableRegion();
  Random random(rtc::TimeMicros());
  for (int i = 0; i < 100; i++) {
    // Make sure we at least draw one pixel.
    int left = random.Rand(rect.left(), rect.right() - 2);
    int top = random.Rand(rect.top(), rect.bottom() - 2);
    drawer->DrawRectangle(
        DesktopRect::MakeLTRB(left, top, random.Rand(left + 1, rect.right()),
                              random.Rand(top + 1, rect.bottom())),
        RgbaColor(random.Rand<uint8_t>(), random.Rand<uint8_t>(),
                  random.Rand<uint8_t>(), random.Rand<uint8_t>()));

    if (i == 50) {
      SleepMs(10000);
    }
  }

  SleepMs(10000);
}

TEST(ScreenDrawerTest, TwoScreenDrawerLocks) {
#if defined (WEBRTC_POSIX)
  // ScreenDrawerLockPosix won't be able to unlink the named semaphore. So use a
  // different semaphore name to avoid deadlock.
  const char* semaphore_name =
      "/global-screen-drawer-linux-8784541a-8120-11e7-88ff-67427b900ef1";
  ScreenDrawerLockPosix::Unlink(semaphore_name);
#else
  // ScreenDrawerLock may not be implemented for all platforms: check its
  // availability first.
  {
    std::unique_ptr<ScreenDrawerLock> lock = ScreenDrawerLock::Create();
    if (!lock) {
      LOG(LS_WARNING) <<
          "No ScreenDrawerLock implementation for current platform.";
      return;
    }
  }
#endif

  const int64_t start_ms = rtc::TimeMillis();
  bool created = false;

  class Task {
   public:
    Task(bool* created, const char* name)
        : created_(created),
          name_(name) {}

    ~Task() = default;

    static void RunTask(void* me) {
      Task* task = static_cast<Task*>(me);
#if defined(WEBRTC_POSIX)
      ScreenDrawerLockPosix lock(task->name_);
#else
      std::unique_ptr<ScreenDrawerLock> lock = ScreenDrawerLock::Create();
#endif
      *(task->created_) = true;
      SleepMs(100);
    }

   private:
    bool* const created_;
    const char* name_;
  } task(&created, semaphore_name);

  rtc::PlatformThread lock_thread(&Task::RunTask, &task, "lock_thread");
  lock_thread.Start();

  // CriticalSection cannot be released in a different thread, which triggers
  // "mutex was not held" error.
  // Event does not implemented for all platforms.
  // So fallback to use a manual loop.
  // TODO(zijiehe): Find a better solution to wait for the creation of the first
  // lock.
  while (!created) {
    SleepMs(10);
  }

#if defined(WEBRTC_POSIX)
  { ScreenDrawerLockPosix lock(semaphore_name); }
#else
  ScreenDrawerLock::Create();
#endif
  ASSERT_GE(100, rtc::TimeMillis() - start_ms);
}

}  // namespace webrtc
