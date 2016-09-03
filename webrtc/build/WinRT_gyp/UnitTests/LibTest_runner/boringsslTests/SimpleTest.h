
// Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_BORINGSSLTESTS_SIMPLETEST_H_
#define WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_BORINGSSLTESTS_SIMPLETEST_H_

namespace BoringSSLTests {
typedef int (*SimpleTestFunction)(void);

// Simple test descriptor
struct SimpleTestDescT {
  wchar_t* pName;  // Test name
  wchar_t* pProject;  // test project
  SimpleTestFunction pFunction;  // pointer to main test function
};

// predefined
class CBoringSSLSimpleTest;

// Boring SSL simple test inserter
template <class TestSolutionProvider>
struct BoringSSLSimpleTestInserter {
  //=======================================================================
  //         Method: BoringSSLSimpleTestInserter
  //    Description: ctor. Inserts all test in array tests to test solution
  //                 provided by TestSolutionProvider
  //       Argument: const SimpleTestDescT * tests - test array,
  //                 !!!! LAST ELEMENT MUST BE NULL !!!!!
  //         return:
  //
  //       History:
  // 2015/06/22 TP: created
  //======================================================================
  explicit BoringSSLSimpleTestInserter(const SimpleTestDescT* tests) {
    int i = 0;
    while (tests[i].pName != NULL) {
      TestSolutionProvider()().AddTest(std::shared_ptr<CBoringSSLSimpleTest>(
        new CBoringSSLSimpleTest(tests[i])));
      ++i;
    }
  }
};

using LibTest_runner::SingleInstanceTestSolutionProvider;
//=============================================================================
//         class: CBoringSSLSimpleTest
//   Description: Simple test executor class for BoringSSL tests
// History:
// 2015/02/27 TP: created
//=============================================================================
class CBoringSSLSimpleTest : public CBoringSSLTestBase {
 private:
  static BoringSSLSimpleTestInserter<SingleInstanceTestSolutionProvider>
    _inserter;
  std::wstring m_Name;
  std::wstring m_Project;
  SimpleTestFunction m_pTestFunction;
 protected:
  int InterchangeableExecute();
 public:
  explicit CBoringSSLSimpleTest(const SimpleTestDescT& desc);
  virtual ~CBoringSSLSimpleTest() {}
  virtual const std::wstring& Name() const { return m_Name; }
  virtual const std::wstring& Project() const { return m_Project; }
  TEST_LIBRARY_IMPL(BoringSSL);
};
}  // namespace BoringSSLTests
#endif  // WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_BORINGSSLTESTS_SIMPLETEST_H_
