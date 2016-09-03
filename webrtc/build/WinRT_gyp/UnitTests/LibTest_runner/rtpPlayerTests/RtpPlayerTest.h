/*
*  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
*
*  Use of this source code is governed by a BSD-style license
*  that can be found in the LICENSE file in the root of the source
*  tree. An additional intellectual property rights grant can be found
*  in the file PATENTS.  All contributing project authors may
*  be found in the AUTHORS file in the root of the source tree.
*/

#ifndef WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_RTPPLAYERTESTS_RTPPLAYERTEST_H_
#define WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_RTPPLAYERTESTS_RTPPLAYERTEST_H_

namespace rtpPlayerTests {

  // class: CRtpPlayerTest
  // executes rtp_player_test project with default parameters
class CRtpPlayerTest :
  public LibTest_runner::CTestBase {
 private:
   AUTO_ADD_TEST(LibTest_runner::SingleInstanceTestSolutionProvider,
    CRtpPlayerTest);
 protected:
    int InterchangeableExecute();
 public:
    virtual ~CRtpPlayerTest() {}
    TEST_NAME_IMPL(RtpPlayerTest);
    TEST_PROJECT_IMPL(rtp_player_test);
    TEST_LIBRARY_IMPL(rtpPlayer);
};

typedef std::shared_ptr<CRtpPlayerTest> SpRtpPlayerTest_t;

}  // namespace rtpPlayerTests

#endif  // WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_RTPPLAYERTESTS_RTPPLAYERTEST_H_
