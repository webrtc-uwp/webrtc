// Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "common.h"
#include "LibSrtpTestBase.h"

extern "C" void getopt_reset();

namespace libSrtpTests {
  void CLibSrtpTestBase::InterchangeablePrepareForExecution() {
    // reset global variables for getopt
    getopt_reset();
  }
}  // namespace libSrtpTests
