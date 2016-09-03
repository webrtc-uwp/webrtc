// Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_TESTSOLUTION_TESTSREPORTERBASE_H_
#define WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_TESTSOLUTION_TESTSREPORTERBASE_H_

namespace LibTest_runner {
// forward declaration
class CTestSolution;
class CTestBase;

#pragma warning(push)
#pragma warning(disable : 4290)

//=============================================================================
//         class: CTestsReporterBase
//   Description: Test reporter base class
// History:
// 2015/03/09 TP: created
//=============================================================================
class CTestsReporterBase {
 public:
  //=======================================================================
  //         Method: Begin
  //    Description: Called when generation of tests report begin
  //                 Implement this method to perform initialization
  //         return: void
  //
  //       History:
  // 2015/03/09 TP: created
  //======================================================================
  virtual void Begin() throw(ReportGenerationException) {}
  //=======================================================================
  //         Method: AddTestSolutionHeader
  //    Description: Implement this method to add Test solution related header
  //       Argument: const CTestSolution & solution test solution
  //         return: void
  //
  //       History:
  // 2015/03/09 TP: created
  //======================================================================
  virtual void AddTestSolutionHeader(const CTestSolution& solution) throw(
                ReportGenerationException) {}
  //=======================================================================
  //         Method: AddTestResult
  //    Description: Implement this method to add specified test result.
  //                 Called for all test in solution
  //       Argument: const CTestBase & test specified test
  //         return: void
  //
  //       History:
  // 2015/03/09 TP: created
  //======================================================================
  virtual void AddTestResult(const CTestBase& test) throw(
              ReportGenerationException) = 0;
  //=======================================================================
  //         Method: AddTestSolutionFooter
  //    Description: Implement this method to add Test solution related footer
  //       Argument: const CTestSolution & solution test solution
  //         return: void
  //
  //       History:
  // 2015/03/09 TP: created
  //======================================================================
  virtual void AddTestSolutionFooter(const CTestSolution& solution) throw(
              ReportGenerationException) {}

  //=======================================================================
  //         Method: End
  //    Description: Called when generation of tests report finishes
  //                 Implement this method to perform finalization
  //         return: void
  //
  //       History:
  // 2015/03/09 TP: created
  //======================================================================
  virtual void End() throw(ReportGenerationException) {}
  virtual ~CTestsReporterBase() {}
};

typedef std::shared_ptr<CTestsReporterBase> SpTestReporter_t;
#pragma warning(pop)
}  // namespace LibTest_runner

#endif  // WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_TESTSOLUTION_TESTSREPORTERBASE_H_
