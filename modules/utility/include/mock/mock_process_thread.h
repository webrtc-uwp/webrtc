/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_UTILITY_INCLUDE_MOCK_MOCK_PROCESS_THREAD_H_
#define MODULES_UTILITY_INCLUDE_MOCK_MOCK_PROCESS_THREAD_H_

#include <memory>

#include "modules/utility/include/process_thread.h"
#include "rtc_base/location.h"
#include "test/gmock.h"

namespace webrtc {

class MockProcessThread : public ProcessThread {
 public:
  MockProcessThread() : ProcessThread("mock") {}
  MOCK_METHOD2(RegisterModule, void(Module* module, const rtc::Location&));
  MOCK_METHOD1(DeRegisterModule, void(Module* module));
};

}  // namespace webrtc
#endif  // MODULES_UTILITY_INCLUDE_MOCK_MOCK_PROCESS_THREAD_H_
