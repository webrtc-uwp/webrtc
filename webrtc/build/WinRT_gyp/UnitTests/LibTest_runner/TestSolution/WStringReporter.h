// Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_TESTSOLUTION_WSTRINGREPORTER_H_
#define WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_TESTSOLUTION_WSTRINGREPORTER_H_

namespace LibTest_runner {
#pragma warning(push)
#pragma warning(disable : 4290)

//=============================================================================
//         class: CWStringReporter
//   Description: Simple string reporter
// History:
// 2015/03/09 TP: created
//=============================================================================
class CWStringReporter : public CTestsReporterBase   {
 public:
  static const int kAllTests = 0x0001;  // include all tests, otherwise only
                                        // executed
  static const int kPrintOutput = 0x0002;  // print test output

 private:
  int m_nFlags;  // flags
  std::shared_ptr<std::wstring> m_spReport;

 public:
  CWStringReporter(int flags = 0);
  virtual ~CWStringReporter() {}
  virtual void AddTestResult(const CTestBase& test) throw(
                                              ReportGenerationException);
  virtual void Begin() throw(ReportGenerationException);
  //=======================================================================
  //         Method: GetReport
  //    Description: Gets generated report
  //         return: std::shared_ptr<std::wstring> pointer to generated report,
  //                 might be null
  //
  //       History:
  // 2015/03/09 TP: created
  //======================================================================
  std::shared_ptr<std::wstring> GetReport() const { return m_spReport; }
};

typedef std::shared_ptr<CWStringReporter> SpWStringReporter_t;

#pragma  warning(pop)
}  // namespace LibTest_runner

#endif  // WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_TESTSOLUTION_WSTRINGREPORTER_H_
