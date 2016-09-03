// Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_LIBSRTPTESTS_RDBXDRIVERTEST_H_
#define WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_LIBSRTPTESTS_RDBXDRIVERTEST_H_
namespace libSrtpTests {
//=============================================================================
//         class: CRdbxDriverValidTest
//   Description: Rdbx_driver validation test. Parameter -v
//                see chromium\src\third_party\libsrtp\Rdbx_driver.vcxproj
// History:
// 2015/02/27 TP: created
//=============================================================================
class CRdbxDriverValidationTest :
  public CLibSrtpTestBase {
  AUTO_ADD_TEST(LibTest_runner::SingleInstanceTestSolutionProvider,
                CRdbxDriverValidationTest);

 protected:
  int InterchangeableExecute();

 public:
  virtual ~CRdbxDriverValidationTest() {}
  TEST_NAME_IMPL(RdbxDriverValidationTest);
  TEST_PROJECT_IMPL(Rdbx_driver);
  TEST_LIBRARY_IMPL(libSrtp);
};

//=============================================================================
//         class: CRdbxDriverTimingTest
//   Description: Rdbx_driver timing test. Parameter -t
//                see chromium\src\third_party\libsrtp\Rdbx_driver.vcxproj
// History:
// 2015/02/27 TP: created
//=============================================================================
class CRdbxDriverTimingTest :
  public CLibSrtpTestBase {
  AUTO_ADD_TEST(LibTest_runner::SingleInstanceTestSolutionProvider,
                CRdbxDriverTimingTest);

 protected:
  int InterchangeableExecute();

 public:
  virtual ~CRdbxDriverTimingTest() {}
  TEST_NAME_IMPL(RdbxDriverTimingTest);
  TEST_PROJECT_IMPL(Rdbx_driver);
  TEST_LIBRARY_IMPL(libSrtp);
};
}  // namespace libSrtpTests

#endif  // WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_LIBSRTPTESTS_RDBXDRIVERTEST_H_
