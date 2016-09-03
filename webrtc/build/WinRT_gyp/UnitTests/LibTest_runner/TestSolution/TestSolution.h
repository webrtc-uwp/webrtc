
// Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_TESTSOLUTION_TESTSOLUTION_H_
#define WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_TESTSOLUTION_TESTSOLUTION_H_

#include <list>

namespace LibTest_runner {
#pragma warning(push)
#pragma warning(disable : 4290)
//=============================================================================
//         class: CTestSolution
//   Description: Class represents test solution
//   !!! Class is not thread safe !!!
//   History:
// 2015/02/27 TP: created
//=============================================================================
class CTestSolution {
 private:
  // hide copy ctor
  CTestSolution(const CTestSolution&);

  // members
  typedef std::list<SpTestBase_t> TestCollection_t;
  typedef std::list<SpTestReporter_t> ReportersCollection_t;

  TestCollection_t      m_Tests;
  ReportersCollection_t m_Reporters;
  bool                  m_Executed;  // true if solution executed
  void InternalTestExecute(const SpTestBase_t& pTest) throw();

 public:
  CTestSolution() {}
  void Execute() throw();
  void Execute(const wchar_t* testName) throw();
  void ExecuteLibrary(const wchar_t* libraryName) throw();
  void AddTest(const SpTestBase_t& ptrTest) throw();
  size_t GetTestCount() const throw();
  //=======================================================================
  //         Method: IsEmpty
  //    Description: Checks whether test suit is empty
  //         return: true if empty
  //
  //       History:
  // 2015/02/27 TP: created
  //======================================================================
  bool IsEmpty() const throw() { return m_Tests.empty(); }
  void AddReporter(const SpTestReporter_t& reporter) throw();
  void GenerateReport() throw(ReportGenerationException);
  void ClearResults() throw();
  //=======================================================================
  //         Method: Executed
  //    Description: Checks weather some test has been executed
  //         return: bool true if some test has been executed
  //
  //       History:
  // 2015/03/09 TP: created
  //======================================================================
  bool Executed() const throw() { return m_Executed; }
};

#pragma warning(pop)
};  // namespace LibTest_runner
#endif  // WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_TESTSOLUTION_TESTSOLUTION_H_
