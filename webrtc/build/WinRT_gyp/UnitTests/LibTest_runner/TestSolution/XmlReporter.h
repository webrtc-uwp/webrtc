
// Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_TESTSOLUTION_XMLREPORTER_H_
#define WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_TESTSOLUTION_XMLREPORTER_H_

namespace LibTest_runner {
#pragma warning(push)
#pragma warning(disable : 4290)

//=============================================================================
//         class: CXmlReporter
//   Description: Test reporter generating xml file
//
// History:
// 2015/03/09 TP: created
//=============================================================================
class CXmlReporter : public CTestsReporterBase {
 public:
  // Print all tests, only executed tests otherwise
  static unsigned int const kAllTests = 1;
 private:
  Platform::String^ OutputFile_;
  Windows::Data::Xml::Dom::XmlDocument^ report_;
  Windows::Data::Xml::Dom::XmlElement^  solutionEl_;
  Windows::Data::Xml::Dom::XmlElement^ GetLibraryElement(
                                              Platform::String^ projectName);
  Windows::Data::Xml::Dom::XmlElement^ GetProjectElement(
                                  Windows::Data::Xml::Dom::XmlElement^ library,
                                  Platform::String^ projectName);
  unsigned int m_nFlags;
 public:
  explicit CXmlReporter(Platform::String^ outputFile, unsigned int flags = 0);
  virtual ~CXmlReporter() {}
  virtual void AddTestResult(const CTestBase& test)
    throw(ReportGenerationException);
  virtual void Begin() throw(ReportGenerationException);
  virtual void AddTestSolutionHeader(const CTestSolution& solution)
    throw(ReportGenerationException);
  virtual void End() throw(ReportGenerationException);
};

typedef std::shared_ptr<CXmlReporter> SpXmlReporter_t;

#pragma warning(pop)
}  // namespace LibTest_runner

#endif  // WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_TESTSOLUTION_XMLREPORTER_H_
