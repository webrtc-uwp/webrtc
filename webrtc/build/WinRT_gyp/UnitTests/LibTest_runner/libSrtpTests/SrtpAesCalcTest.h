// Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_LIBSRTPTESTS_SRTPAESCALCTEST_H_
#define WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_LIBSRTPTESTS_SRTPAESCALCTEST_H_

namespace libSrtpTests {
//=============================================================================
//         class: CSrtpAesCalcTest
//   Description: class executes replay_driver test project,
//              see chromium\src\third_party\libsrtp\srtp_test_aes_calc.vcxproj
// History:
// 2015/02/27 TP: created
//=============================================================================
class CSrtpAesCalcTest :
  public CLibSrtpTestBase {
 private:
   AUTO_ADD_TEST(LibTest_runner::SingleInstanceTestSolutionProvider, CSrtpAesCalcTest);

 protected:
  int InterchangeableExecute();

 public:
  virtual ~CSrtpAesCalcTest() {}
  TEST_NAME_IMPL(SrtpAesCalcTest);
  TEST_PROJECT_IMPL(srtp_test_aes_calc);
  TEST_LIBRARY_IMPL(libSrtp)

    virtual void InterchangeableVerifyResult();
};

typedef std::shared_ptr<CSrtpAesCalcTest> SpSrtpAesCalcTest_t;
}  // namespace libSrtpTests

#endif  // WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_LIBSRTPTESTS_SRTPAESCALCTEST_H_
