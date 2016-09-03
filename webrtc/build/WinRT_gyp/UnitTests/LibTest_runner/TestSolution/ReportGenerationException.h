
// Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_TESTSOLUTION_REPORTGENERATIONEXCEPTION_H_
#define WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_TESTSOLUTION_REPORTGENERATIONEXCEPTION_H_

namespace LibTest_runner {
//=============================================================================
//         class: ReportGenerationException
//   Description: thrown when errors appears during generation tests report
// History:
// 2015/03/09 TP: created
//=============================================================================
class ReportGenerationException : public std::exception {
 private:
  Platform::Exception^ InnerException_;

 public:
  //=======================================================================
  //         Method: ReportGenerationException
  //    Description: cotr
  //       Argument: const char * const & message - exception message
  //         return:
  //
  //       History:
  // 2015/03/17 TP: created
  //======================================================================
  ReportGenerationException(const char * const &message)
    : std::exception(message) {}

  //=======================================================================
  //         Method: ReportGenerationException
  //    Description: ctor
  //       Argument: Platform::Exception ^ innerEx - inner exception
  //                 The purpose of inner exception is to handle exception
  //                 from Windows Runtime API
  //         return:
  //
  //       History:
  // 2015/03/17 TP: created
  //======================================================================
  ReportGenerationException(Platform::Exception^ innerEx)
    :InnerException_(innerEx) {}

  //=======================================================================
  //         Method: GetInnerException
  //    Description: Gets inner exception.
  //                 The purpose of inner exception is to handle exception
  //                 from Windows Runtime API
  //         return: Platform::Exception^
  //
  //       History:
  // 2015/03/17 TP: created
  //======================================================================
  Platform::Exception^ GetInnerException() const { return InnerException_; }
};
}  // namespace LibTest_runner

#endif  // WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_TESTSOLUTION_REPORTGENERATIONEXCEPTION_H_
