// Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_OPUSTESTS_OPUSDECODETEST_H_
#define WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_OPUSTESTS_OPUSDECODETEST_H_

namespace OpusTests {
//=============================================================================
//         class: COpusDecodeTest
//   Description: class executes opus decode test project,
//                see chromium\src\third_party\opus\test_opus_decode.vcxproj
// History:
// 2015/02/27 TP: created
//=============================================================================
class COpusDecodeTest :
  public COpusTestBase {
 private:
   AUTO_ADD_TEST(LibTest_runner::SingleInstanceTestSolutionProvider,
                  COpusDecodeTest);

 protected:
  int InterchangeableExecute();

 public:
  virtual ~COpusDecodeTest() {}
  TEST_NAME_IMPL(OpusDecodeTest);
  TEST_PROJECT_IMPL(test_opus_decode);
  TEST_LIBRARY_IMPL(Opus);
};

typedef std::shared_ptr<COpusDecodeTest> SpOpusDecodeTest_t;
}  // namespace OpusTests

#endif  // WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_OPUSTESTS_OPUSDECODETEST_H_
