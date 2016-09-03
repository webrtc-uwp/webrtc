// Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_LIBSRTPTESTS_SRTPCIPHERDRIVERTEST_H_
#define WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_LIBSRTPTESTS_SRTPCIPHERDRIVERTEST_H_

namespace libSrtpTests {
//=============================================================================
//         class: CSrtpCipherDriverValidationTest
//   Description: class executes replay_driver test project,
//         see chromium\src\third_party\libsrtp\srtp_test_cipher_driver.vcxproj
// History:
// 2015/02/27 TP: created
//=============================================================================
class CSrtpCipherDriverValidationTest :
  public CLibSrtpTestBase {
 private:
   AUTO_ADD_TEST(LibTest_runner::SingleInstanceTestSolutionProvider,
                CSrtpCipherDriverValidationTest);

 protected:
  int InterchangeableExecute();

 public:
  virtual ~CSrtpCipherDriverValidationTest() {}
  TEST_NAME_IMPL(SrtpCipherDriverValidationTest);
  TEST_PROJECT_IMPL(srtp_test_cipher_driver);
  TEST_LIBRARY_IMPL(libSrtp);
};

//=============================================================================
//         class: CSrtpCipherDriverTimingTest
//   Description: class executes replay_driver test project,
//         see chromium\src\third_party\libsrtp\srtp_test_cipher_driver.vcxproj
// History:
// 2015/02/27 TP: created
//=============================================================================
class CSrtpCipherDriverTimingTest :
  public CLibSrtpTestBase {
 private:
   AUTO_ADD_TEST(LibTest_runner::SingleInstanceTestSolutionProvider,
                CSrtpCipherDriverTimingTest);

 protected:
  int InterchangeableExecute();

 public:
  virtual ~CSrtpCipherDriverTimingTest() {}
  TEST_NAME_IMPL(SrtpCipherDriverTimingTest);
  TEST_PROJECT_IMPL(srtp_test_cipher_driver);
  TEST_LIBRARY_IMPL(libSrtp);

  virtual void InterchangeableVerifyResult();
};

//=============================================================================
//         class: CSrtpCipherDriverArrayTimingTest
//   Description: class executes replay_driver test project,
//         see chromium\src\third_party\libsrtp\srtp_test_cipher_driver.vcxproj
// History:
// 2015/02/27 TP: created
//=============================================================================
class CSrtpCipherDriverArrayTimingTest :
  public CLibSrtpTestBase {
 private:
   AUTO_ADD_TEST(LibTest_runner::SingleInstanceTestSolutionProvider,
                CSrtpCipherDriverArrayTimingTest);

 protected:
  int InterchangeableExecute();

 public:
  virtual ~CSrtpCipherDriverArrayTimingTest() {}
  TEST_NAME_IMPL(SrtpCipherDriverArrayTimingTest);
  TEST_PROJECT_IMPL(srtp_test_cipher_driver);
  TEST_LIBRARY_IMPL(libSrtp);

  virtual void InterchangeableVerifyResult();
};
}  // namespace libSrtpTests

#endif  // WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_LIBSRTPTESTS_SRTPCIPHERDRIVERTEST_H_
