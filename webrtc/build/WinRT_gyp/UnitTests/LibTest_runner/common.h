
// Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_COMMON_H_
#define WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_COMMON_H_

#include <collection.h>
#include <ppltasks.h>

// STL includes
#include <string>
#include <vector>
#include <exception>
#include <chrono>

// Helpers
#include "Helpers/SafeSingleton.h"
#include "Helpers/TestInserter.h"
#include "Helpers/StdOutputRedirector.h"

// Test Solution
#include "TestSolution/ReportGenerationException.h"
#include "TestSolution/TestsReporterBase.h"
#include "TestSolution/TestBase.h"
#include "TestSolution/TestSolution.h"
#include "TestSolution/WStringReporter.h"
#include "TestSolution/XmlReporter.h"
#include "TestSolution\TestSolutionProvider.h"


// libsrtp tests
#include "libSrtpTests\LibSrtpTestBase.h"
#include "libSrtpTests\ReplayDriverTest.h"
#include "libSrtpTests\RocDriverTest.h"
#include "libSrtpTests\RtpwTest.h"
#include "libSrtpTests\RdbxDriverTest.h"
#include "libSrtpTests\SrtpDriverTest.h"
#include "libSrtpTests\SrtpAesCalcTest.h"
#include "libSrtpTests\SrtpCipherDriverTest.h"
#include "libSrtpTests\SrtpDatatypesDriverTest.h"
#include "libSrtpTests\SrtpEnvTest.h"
#include "libSrtpTests\SrtpKernelDriverTest.h"
#include "libSrtpTests\SrtpRandGenTest.h"
#include "libSrtpTests\SrtpSha1DriverTest.h"
#include "libSrtpTests\SrtpStatDriverTest.h"

// opus tests
#include "opusTests/OpusTestBase.h"
#include "opusTests/OpusEncodeTest.h"
#include "opusTests/OpusDecodeTest.h"
#include "opusTests/OpusPaddingTest.h"
#include "opusTests/OpusApiTest.h"

// rtpPlayer tests
#include "rtpPlayerTests/RtpPlayerTestBase.h"
#include "rtpPlayerTests/RtpPlayerTest.h"

// BoringSSL
#include "boringsslTests/BoringSslTestBase.h"
#include "boringsslTests/SimpleTest.h"

#endif  // WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_COMMON_H_
