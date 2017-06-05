/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_UTILITY_SIMULCAST_UNITTEST_COMMON_H_
#define WEBRTC_MODULES_VIDEO_CODING_UTILITY_SIMULCAST_UNITTEST_COMMON_H_

namespace webrtc {
namespace testing {

const int kDefaultWidth = 1280;
const int kDefaultHeight = 720;
const int kNumberOfSimulcastStreams = 3;
const int kColorY = 66;
const int kColorU = 22;
const int kColorV = 33;
const int kMaxBitrates[kNumberOfSimulcastStreams] = {150, 600, 1200};
const int kMinBitrates[kNumberOfSimulcastStreams] = {50, 150, 600};
const int kTargetBitrates[kNumberOfSimulcastStreams] = {100, 450, 1000};

template <typename T>
void SetExpectedValues3(T value0, T value1, T value2, T* expected_values) {
  expected_values[0] = value0;
  expected_values[1] = value1;
  expected_values[2] = value2;
}

enum PlaneType {
  kYPlane = 0,
  kUPlane = 1,
  kVPlane = 2,
  kNumOfPlanes = 3,
};

}  // namespace testing
}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_UTILITY_SIMULCAST_UNITTEST_COMMON_H_
