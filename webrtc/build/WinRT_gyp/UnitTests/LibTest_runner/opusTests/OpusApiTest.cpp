// Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "common.h"


// test entry point declaration
extern "C" int opus_api_main(int _argc, char **_argv);

namespace OpusTests {

  AUTO_ADD_TEST_IMPL(COpusApiTest);

  int COpusApiTest::InterchangeableExecute() {
    char* argv[] = { "." };
    return opus_api_main(1, argv);
  }
}  // namespace OpusTests

