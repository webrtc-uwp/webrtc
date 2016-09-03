/*
*  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
*
*  Use of this source code is governed by a BSD-style license
*  that can be found in the LICENSE file in the root of the source
*  tree. An additional intellectual property rights grant can be found
*  in the file PATENTS.  All contributing project authors may
*  be found in the AUTHORS file in the root of the source tree.
*/

#ifndef WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_TESTSOLUTION_TESTBASE_H_
#define WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_TESTSOLUTION_TESTBASE_H_

namespace LibTest_runner {

//=============================================================================
//         class: CTestBase
//   Description: Class provides basic test functionality
// History:
// 2015/02/27 TP: created
//=============================================================================
class CTestBase {
 private:
  // hide copy ctor
  CTestBase(const CTestBase&);
  void PrepareForExecution();
  void VerifyResult();
  bool           m_bExecuted;  // true if test was executed
  bool           m_bSucceed;  // true if test succeed
  int            m_nExitStatus;  // test exit code (from main method)
  std::chrono::milliseconds  m_uExecutionTimeMs;  // Test execution time in
                                                  // milliseconds seconds

 protected:
  std::wstring   m_wsOutput;  // test output
  std::wstring   m_wsResultMessage;  // result message, e.g. explaining why
                                     // test failed
  //=======================================================================
  //         Method: SetSucceed
  //    Description: Set succeed test status
  //       Argument: bool succeed - succeed status
  //         return: void
  //
  //       History:
  // 2015/03/10 TP: created
  //======================================================================
  void SetSucceed(bool succeed) { m_bSucceed = succeed; }

  virtual int    InterchangeableExecute() = 0;
  //=======================================================================
  //         Method: InterchangeablePrepareForExecution
  //    Description: Implement this method to do special test preparations
  //         return: void
  //
  //       History:
  // 2015/03/06 TP: created
  //======================================================================
  virtual void InterchangeablePrepareForExecution() {}
  //=======================================================================
  //         Method: InterchangeableTestCleanup
  //    Description: Implement this method to do special test cleanup
  //         return: void
  //
  //       History:
  // 2015/03/06 TP: created
  //======================================================================
  virtual void InterchangeableTestCleanup() {}

  //=======================================================================
  //         Method: InterchangeableVerifyResult
  //    Description: Implement this method to do special result verification
  //                 Don't forget to set m_bSucceed
  //         return: void
  //
  //       History:
  // 2015/03/06 TP: created
  //======================================================================
  virtual void InterchangeableVerifyResult() {}

  //=======================================================================
  //         Method: OutputBufferSize
  //    Description: Returns Output buffer size
  //                 Overwrites this method if needed different value
  //         return: size_t Output buffer size
  //
  //       History:
  // 2015/03/06 TP: created
  //======================================================================
  virtual size_t OutputBufferSize() const { return 1024 * 1024; /*1MB*/ }

 public:
  CTestBase();
  virtual ~CTestBase() {}
  int Execute();
  //=======================================================================
  //         Method: Name
  //    Description: Returns test name
  //         return: const std::wstring& test name
  //
  //       History:
  // 2015/03/06 TP: created
  //======================================================================
  virtual const std::wstring& Name() const = 0;

  //=======================================================================
  //         Method: Project
  //    Description: Returns Test project
  //         return: const std::wstring& test project
  //
  //       History:
  // 2015/03/09 TP: created
  //======================================================================
  virtual const std::wstring& Project() const = 0;

  //=======================================================================
  //         Method: Library
  //    Description: Returns library name
  //         return: const std::wstring& library name
  //
  //       History:
  // 2015/03/09 TP: created
  //======================================================================
  virtual const std::wstring& Library() const = 0;
  //=======================================================================
  //         Method: Output
  //    Description: Returns test output (from standard output)
  //         return: const std::wstring& console output
  //
  //       History:
  // 2015/03/06 TP: created
  //======================================================================
  const std::wstring& Output() const { return m_wsOutput; }

  //=======================================================================
  //         Method: ResultMessage
  //    Description: Returns result message, e.g. describing reason why
  //                  test fails
  //         return: const std::wstring& result message
  //
  //       History:
  // 2015/03/10 TP: created
  //======================================================================
  const std::wstring& ResultMessage() const { return  m_wsResultMessage; }
  //=======================================================================
  //         Method: ExitStatus
  //    Description: Returns test exist status
  //         return: int exit status
  //
  //       History:
  // 2015/03/06 TP: created
  //======================================================================
  int ExitStatus() const { return m_nExitStatus;  }
  //=======================================================================
  //         Method: Succeed
  //    Description: Returns success state
  //         return: bool true if success
  //
  //       History:
  // 2015/03/06 TP: created
  //======================================================================
  bool Succeed() const { return m_bSucceed;  }

  //=======================================================================
  //         Method: Failed
  //    Description: return true if test failed
  //         return: bool true if test failed
  //
  //       History:
  // 2015/03/10 TP: created
  //======================================================================
  bool Failed() const { return !m_bSucceed; }
  void Reset();

  //=======================================================================
  //         Method: Executed
  //    Description: Returns true if test was executed
  //         return: bool true if test was executed
  //
  //       History:
  // 2015/03/09 TP: created
  //======================================================================
  bool Executed() const { return m_bExecuted; }

  //=======================================================================
  //         Method: GetExecutionTimeMs
  //    Description: Test execution Time in milliseconds
  //         return: std::chrono::milliseconds
  //
  //       History:
  // 2015/05/07 TP: created
  //======================================================================
  std::chrono::milliseconds GetExecutionTimeMs() const {
    return m_uExecutionTimeMs;
  }
};

typedef std::shared_ptr<CTestBase> SpTestBase_t;

// This marco simplifies implementation of CTestBase::Name()
#define TEST_NAME_IMPL(NAME) \
  virtual const std::wstring& Name() const\
    { \
    static std::wstring name = L#NAME; \
    return name; \
  }

// This marco simplifies implementation of CTestBase::Project()
#define TEST_PROJECT_IMPL(PROJECT) \
  virtual const std::wstring& Project() const\
        { \
    static std::wstring project = L#PROJECT; \
    return project; \
  }

// This marco simplifies implementation of CTestBase::Library()
#define TEST_LIBRARY_IMPL(LIBRARY) \
  virtual const std::wstring& Library() const\
        { \
    static std::wstring library = L#LIBRARY; \
    return library; \
  }
}  // namespace LibTest_runner

#endif  // WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_TESTSOLUTION_TESTBASE_H_

