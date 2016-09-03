// Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "common.h"
#include "SrtpSha1DriverTest.h"

// test entry point declaration
extern "C" int srtp_test_sha1_driver_main();

AUTO_ADD_TEST_IMPL(libSrtpTests::CSrtpSha1DriverTest);

int libSrtpTests::CSrtpSha1DriverTest::InterchangeableExecute() {
  return srtp_test_sha1_driver_main();
}

