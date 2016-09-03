
// Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_BORINGSSLTESTS_BORINGSSLTESTBASE_H_
#define WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_BORINGSSLTESTS_BORINGSSLTESTBASE_H_

namespace BoringSSLTests {
using LibTest_runner::CTestBase;
class CBoringSSLTestBase : public CTestBase {
  virtual void InterchangeableVerifyResult();
};
}

#endif  // WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_BORINGSSLTESTS_BORINGSSLTESTBASE_H_
