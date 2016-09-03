// Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "common.h"
#include "RdbxDriverTest.h"

// test entry point declaration
extern "C" int rdbx_driver_main(int argc, char *argv[]);

AUTO_ADD_TEST_IMPL(libSrtpTests::CRdbxDriverValidationTest);
AUTO_ADD_TEST_IMPL(libSrtpTests::CRdbxDriverTimingTest);

namespace libSrtpTests {
  //==============================================================
  // CRdbxDriverValidationTest

  int CRdbxDriverValidationTest::InterchangeableExecute() {
    char* argv[] = { ".", "-v"};
    return rdbx_driver_main(2, argv);
  }

  //==============================================================
  // CRdbxDriverTimingTest

  int CRdbxDriverTimingTest::InterchangeableExecute() {
     char* argv[] = { ".", "-t" };
    return rdbx_driver_main(2, argv);
  }
}  // namespace libSrtpTests
