/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/win/screen_capture_utils.h"

#include <vector>
#include <string>

#include "webrtc/modules/desktop_capture/desktop_capturer.h"
#include "webrtc/test/gtest.h"

namespace webrtc {

TEST(ScreenCaptureUtilsTest, GetScreenList) {
  DesktopCapturer::SourceList screens;
  std::vector<std::string> device_names;

  ASSERT_TRUE(GetScreenList(&screens));
  screens.clear();
  ASSERT_TRUE(GetScreenList(&screens, &device_names));

  ASSERT_EQ(screens.size(), device_names.size());
}

// This test cannot ensure GetScreenListFromDeviceNames() won't reorder the
// devices in its output, since the device name is missing.
TEST(ScreenCaptureUtilsTest, GetScreenListFromDeviceNamesAndGetIndex) {
  const std::vector<std::string> device_names = {
    "\\\\.\\DISPLAY0",
    "\\\\.\\DISPLAY1",
    "\\\\.\\DISPLAY2",
  };
  DesktopCapturer::SourceList screens;
  ASSERT_TRUE(GetScreenListFromDeviceNames(device_names, &screens));
  ASSERT_EQ(device_names.size(), screens.size());

  for (size_t i = 0; i < screens.size(); i++) {
    int index;
    ASSERT_TRUE(GetIndexFromScreenId(screens[i].id, device_names, &index));
    ASSERT_EQ(index, static_cast<int>(i));
  }
}

}  // namespace webrtc
