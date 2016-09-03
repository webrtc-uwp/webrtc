// Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "common.h"
#include "SrtpCipherDriverTest.h"

// test entry point declaration
extern "C" int srtp_test_cipher_driver_main(int argc, char *argv[]);

AUTO_ADD_TEST_IMPL(libSrtpTests::CSrtpCipherDriverValidationTest);
AUTO_ADD_TEST_IMPL(libSrtpTests::CSrtpCipherDriverTimingTest);
AUTO_ADD_TEST_IMPL(libSrtpTests::CSrtpCipherDriverArrayTimingTest);

namespace libSrtpTests {

  //=================================================================
  // CSrtpCipherDriverValidationTest

  int CSrtpCipherDriverValidationTest::InterchangeableExecute() {
    char* argv[] = { ".", "-v" };

    return srtp_test_cipher_driver_main(2, argv);
  }

  //=================================================================
  // CSrtpCipherDriverTimingTest
  int CSrtpCipherDriverTimingTest::InterchangeableExecute() {
    char* argv[] = { ".", "-t" };

    return srtp_test_cipher_driver_main(2, argv);
  }

  void CSrtpCipherDriverTimingTest::InterchangeableVerifyResult() {
    // Right now there is not "success" check. Timing test just measures
    // number of operation per second. Right now we have no requirements.
  }

  //=================================================================
  // CSrtpCipherDriverArrayTimingTest
  int CSrtpCipherDriverArrayTimingTest::InterchangeableExecute() {
    char* argv[] = { ".", "-a" };

    return srtp_test_cipher_driver_main(2, argv);
  }

  void CSrtpCipherDriverArrayTimingTest::InterchangeableVerifyResult() {
    // Right now there is not "success" check. Timing test just measures
    // number of operation per second. Right now we have no requirements.
  }
}  // namespace libSrtpTests

