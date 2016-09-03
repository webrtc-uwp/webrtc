// Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "common.h"

namespace LibTest_runner {

#pragma warning(push)
#pragma warning(disable : 4290)

//=======================================================================
//         Method: CWStringReporter
//    Description: ctor
//       Argument: int flags - reporter flags
//         return:
//
//       History:
// 2015/03/09 TP: created
//======================================================================
CWStringReporter::CWStringReporter(int flags)
  : m_nFlags(flags) {
}

void CWStringReporter::AddTestResult(const CTestBase& test) {
  if (test.Executed() || (m_nFlags & kAllTests)) {
    // in case output is print add separator to better reading experience
    if (m_nFlags & kPrintOutput) {
      (*m_spReport) += L"========== Begin Test ============\n";
    }

    // print test name
    (*m_spReport) += L"Project: " + test.Library();
    (*m_spReport) += L"::" + test.Project() + L"\t Name: ";
    (*m_spReport) += test.Name() + L"\n";

    if (m_nFlags & kPrintOutput) {
      (*m_spReport) += L"----------- Begin Console Output ----------\n";
      (*m_spReport) += test.Output();
      (*m_spReport) += L"\n---------- End Console Output ----------\n";
    }

    (*m_spReport) += L"\tResult: ";
    (*m_spReport) += test.Succeed() ? L"Pass" : L"Failed";
    (*m_spReport) += L"\tExit status: ";
    (*m_spReport) += std::to_wstring(test.ExitStatus());
    (*m_spReport) += L"\tExecution Time (ms): ";
    (*m_spReport) += std::to_wstring(test.GetExecutionTimeMs().count());
    if (!test.ResultMessage().empty()) {
      (*m_spReport) += L"\n\tResult Message: ";
      (*m_spReport) += test.ResultMessage();
    }
    (*m_spReport) += L"\n";

    // in case output is print add separator to better reading experience
    if (m_nFlags & kPrintOutput) {
      (*m_spReport) += L"========== End Test ============\n\n";
    }
  }
}

void CWStringReporter::Begin() throw(ReportGenerationException) {
  m_spReport.reset(new std::wstring());
}

#pragma warning(pop)
}  // namespace LibTest_runner
