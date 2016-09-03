// Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_OPUSTESTS_OPUSENCODETEST_H_
#define WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_OPUSTESTS_OPUSENCODETEST_H_

namespace OpusTests {
//=============================================================================
//         class: COpusEncodeTest
//   Description: class executes replay_driver test project,
//                see chromium\src\third_party\opus\test_opus_encode.vcxproj
// History:
// 2015/02/27 TP: created
//=============================================================================
class COpusEncodeTest :
  public COpusTestBase {
 private:
   AUTO_ADD_TEST(LibTest_runner::SingleInstanceTestSolutionProvider,
                COpusEncodeTest);

 protected:
  int InterchangeableExecute();

 public:
  virtual ~COpusEncodeTest() {}
  TEST_NAME_IMPL(OpusEncodeTest);
  TEST_PROJECT_IMPL(test_opus_encode);
  TEST_LIBRARY_IMPL(Opus);
};

typedef std::shared_ptr<COpusEncodeTest> SpOpusEncodeTest_t;
}  // namespace OpusTests

#endif  // WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_OPUSTESTS_OPUSENCODETEST_H_
