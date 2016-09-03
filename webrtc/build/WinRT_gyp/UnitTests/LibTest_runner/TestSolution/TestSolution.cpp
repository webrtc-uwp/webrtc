
// Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "webrtc/build/WinRT_gyp/UnitTests/LibTest_runner/common.h"
#include "webrtc/build/WinRT_gyp/UnitTests/LibTest_runner/TestSolution/TestSolution.h"

#pragma warning(push)
#pragma warning(disable : 4290)

namespace LibTest_runner {

//=======================================================================
//         Method: InternalTestExecute
//    Description: Runs specified test
//       Argument: const SpTestBase_t & pTest test to run
//         return: void
//
//       History:
// 2015/03/03 TP: created
//======================================================================
void CTestSolution::InternalTestExecute(const SpTestBase_t& pTest) {
  m_Executed = true;
  try {
    wprintf(L"\n--- Executing %s ------\n", pTest->Name().c_str());
    pTest->Execute();
  }
  catch (int status) {
    wprintf(L"--- %s test terminated with status %d ------\n",
      pTest->Name().c_str(), status);
  }
}

//=======================================================================
//         Method: CTestSolution::Execute
//    Description: executes test solution
//         return: void
//
//       History:
// 2015/02/27 TP: created
//======================================================================
void CTestSolution::Execute() {
  if (!IsEmpty()) {
    for (auto it = m_Tests.cbegin(); it != m_Tests.cend(); ++it) {
      InternalTestExecute(*it);
    }
  }
}


//=======================================================================
//         Method: Execute
//    Description: Executes test with specified name
//       Argument: const wchar_t * testName - specified test name
//         return: void
//
//       History:
// 2015/03/03 TP: created
//======================================================================
void CTestSolution::Execute(const wchar_t* testName) {
  if (!IsEmpty()) {
    for (auto it = m_Tests.cbegin(); it != m_Tests.cend(); ++it) {
      if ((*it)->Name().compare(testName) == 0) {
        InternalTestExecute(*it);
      }
    }
  }
}

//=======================================================================
//         Method: Execute
//    Description: Executes test from specified library
//       Argument: const wchar_t * libraryName - specified library name
//         return: void
//
//       History:
// 2015/03/03 TP: created
//======================================================================
void CTestSolution::ExecuteLibrary(const wchar_t* libraryName) {
  if (!IsEmpty()) {
    for (auto it = m_Tests.cbegin(); it != m_Tests.cend(); ++it) {
      if ((*it)->Library().compare(libraryName) == 0) {
        InternalTestExecute(*it);
      }
    }
  }
}

//=======================================================================
//         Method: LibTest_runner::CTestSolution::AddTest
//    Description: Adds test to solution
//       Argument: const SpTestBase_t & ptrTest - test to add
//         return:
//
//       History:
// 2015/02/27 TP: created
//======================================================================
void CTestSolution::AddTest(const SpTestBase_t& ptrTest) {
  m_Tests.push_back(ptrTest);
}

//=======================================================================
//         Method: LibTest_runner::CTestSolution::GetTestCount
//    Description: Gets test count
//         return: size_t test count
//
//       History:
// 2015/02/27 TP: created
//======================================================================
size_t CTestSolution::GetTestCount() const {
  size_t ret = 0;
  if (!IsEmpty()) {
    ret = m_Tests.size();
  }

  return ret;
}

//=======================================================================
//         Method: AddReporter
//    Description: Adds test reporter
//       Argument: const SpTestReporter_t & reporter reporter to add
//         return: void
//
//       History:
// 2015/03/09 TP: created
//======================================================================
void CTestSolution::AddReporter(const SpTestReporter_t& reporter) {
  m_Reporters.push_back(reporter);
}

//=======================================================================
//         Method: GenerateReport
//    Description: Generates reports, using all registered reporters.
//                 !! Only if Executed() = true !!.
//         return: void
//
//       History:
// 2015/03/09 TP: created
//======================================================================
void CTestSolution::GenerateReport() throw(ReportGenerationException) {
  if (m_Executed) {
    // got trough all reporters
    for (auto reporter = m_Reporters.cbegin(); reporter !=
      m_Reporters.cend(); ++reporter) {
      (*reporter)->Begin();
      (*reporter)->AddTestSolutionHeader(*this);
      // add all tests
      for (auto test = m_Tests.cbegin(); test != m_Tests.cend(); ++test) {
        (*reporter)->AddTestResult(**test);
      }

      (*reporter)->AddTestSolutionFooter(*this);
      (*reporter)->End();
    }
  }
}

//=======================================================================
//         Method: ClearResults
//    Description: Clears tests results (preparation for next execution)
//         return: void
//
//       History:
// 2015/03/09 TP: created
//======================================================================
void CTestSolution::ClearResults() throw() {
  for (auto it = m_Tests.begin(); it != m_Tests.end(); ++it) {
    (*it)->Reset();
  }

  m_Executed = false;
}

}  // namespace LibTest_runner
#pragma warning(pop)
