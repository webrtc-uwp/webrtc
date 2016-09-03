// Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_LIBSRTPTESTS_LIBSRTPTESTBASE_H_
#define WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_LIBSRTPTESTS_LIBSRTPTESTBASE_H_

namespace libSrtpTests {

//=============================================================================
//         class: CLibSrtpTestBase
//   Description: Base class for LibSrtp test
// History:
// 2015/03/10 TP: created
//=============================================================================
class CLibSrtpTestBase : public LibTest_runner::CTestBase {
 public:
  virtual ~CLibSrtpTestBase() {}
  virtual void InterchangeablePrepareForExecution();
};
}  // namespace libSrtpTests

#endif  // WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_LIBSRTPTESTS_LIBSRTPTESTBASE_H_
