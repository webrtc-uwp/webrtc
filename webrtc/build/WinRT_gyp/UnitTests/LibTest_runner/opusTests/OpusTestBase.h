// Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_OPUSTESTS_OPUSTESTBASE_H_
#define WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_OPUSTESTS_OPUSTESTBASE_H_

namespace OpusTests {

//=============================================================================
//         class: COpusTestBase
//   Description: Base class for Opus tests
// History:
// 2015/03/10 TP: created
//=============================================================================
class COpusTestBase : public LibTest_runner::CTestBase {
 public:
  virtual ~COpusTestBase() {}
  virtual void InterchangeableVerifyResult();
};
}  // namespace OpusTests

#endif  // WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_OPUSTESTS_OPUSTESTBASE_H_
