/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/state_monitor.h"
#include "webrtc/test/gtest.h"

namespace webrtc {
namespace {

struct TestStruct {
  TestStruct() : TestStruct(0) {}
  explicit TestStruct(int value) : value(value) {}

  bool equals(const TestStruct& other) const { return value == other.value; }

  int value;
};

class TestStateMonitor : public StateMonitor<TestStruct> {
 public:
  bool Set(const TestStruct& value) {
    return StateMonitor<TestStruct>::Set(value);
  }
};

}  // namespace

TEST(StateMonitorTest, Test) {
  TestStateMonitor monitor;
  ASSERT_FALSE(monitor.Set(TestStruct(1)));
  ASSERT_EQ(1, monitor.last().value);
  ASSERT_FALSE(monitor.Set(TestStruct(1)));
  ASSERT_EQ(1, monitor.last().value);
  ASSERT_TRUE(monitor.Set(TestStruct(2)));
  ASSERT_EQ(2, monitor.last().value);
  ASSERT_FALSE(monitor.Set(TestStruct(2)));
  ASSERT_EQ(2, monitor.last().value);
  monitor.Reset();
  ASSERT_FALSE(monitor.Set(TestStruct(3)));
  ASSERT_EQ(3, monitor.last().value);
  ASSERT_FALSE(monitor.Set(TestStruct(3)));
  ASSERT_EQ(3, monitor.last().value);
  ASSERT_TRUE(monitor.Set(TestStruct(4)));
  ASSERT_EQ(4, monitor.last().value);
}

}  // namespace webrtc
