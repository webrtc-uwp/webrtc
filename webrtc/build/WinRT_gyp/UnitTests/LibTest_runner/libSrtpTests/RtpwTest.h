// Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_LIBSRTPTESTS_RTPWTEST_H_
#define WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_LIBSRTPTESTS_RTPWTEST_H_

namespace libSrtpTests {
//=============================================================================
//         class: CRtpwTest
//   Description: class executes rtpw test project,
//                see chromium\src\third_party\libsrtp\rtpw.vcxproj
// History:
// 2015/02/27 TP: created
//=============================================================================
class CRtpwTest :
  public CLibSrtpTestBase {
  AUTO_ADD_TEST(LibTest_runner::SingleInstanceTestSolutionProvider,
              CRtpwTest);

 protected:
  int InterchangeableExecute();

 public:
  virtual ~CRtpwTest() {}
  TEST_NAME_IMPL(CRtpwTest);
  TEST_PROJECT_IMPL(rtpw);
  TEST_LIBRARY_IMPL(libSrtp);
};
}  // namespace libSrtpTests

#endif  // WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_LIBSRTPTESTS_RTPWTEST_H_
