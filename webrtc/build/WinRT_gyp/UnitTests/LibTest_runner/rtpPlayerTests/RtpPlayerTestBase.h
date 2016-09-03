/*
*  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
*
*  Use of this source code is governed by a BSD-style license
*  that can be found in the LICENSE file in the root of the source
*  tree. An additional intellectual property rights grant can be found
*  in the file PATENTS.  All contributing project authors may
*  be found in the AUTHORS file in the root of the source tree.
*/

#ifndef WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_RTPPLAYERTESTS_RTPPLAYERTESTBASE_H_
#define WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_RTPPLAYERTESTS_RTPPLAYERTESTBASE_H_

namespace RtpPlayerTests {

//==========================================================================
//         class: CRtpPlayerTestBase
//   Description: Base class for rtp_Player tests
//==========================================================================
class CRtpPlayerTestBase : public LibTest_runner::CTestBase {
 public:
    virtual ~CRtpPlayerTestBase() {}
    virtual void InterchangeableVerifyResult();
};
}  // namespace RtpPlayerTests

#endif  // WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_RTPPLAYERTESTS_RTPPLAYERTESTBASE_H_
