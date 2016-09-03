// Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "common.h"
#include "RtpwTest.h"

// test entry point declaration
extern "C" int rtpw_main(int argc, char *argv[]);

AUTO_ADD_TEST_IMPL(libSrtpTests::CRtpwTest);

namespace libSrtpTests {

  int CRtpwTest::InterchangeableExecute() {
    // Configuration has to be somehow handled.
    // Rtpw act as sender and receiver, both has to be launched in same time.
    // We probably needs two threads here.
    char* argv[] = { "-s", "127.0.0.1", "100" };
    return rtpw_main(3, argv);
  }

}  // namespace libSrtpTests
