/*
*  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
*
*  Use of this source code is governed by a BSD-style license
*  that can be found in the LICENSE file in the root of the source
*  tree. An additional intellectual property rights grant can be found
*  in the file PATENTS.  All contributing project authors may
*  be found in the AUTHORS file in the root of the source tree.
*/

#include "common.h"
#include "RtpPlayerTest.h"

// test entry point declaration
extern int video_coding_tester_main(int argc, char **argv);

AUTO_ADD_TEST_IMPL(rtpPlayerTests::CRtpPlayerTest);

int rtpPlayerTests::CRtpPlayerTest::InterchangeableExecute() {
  char* argv[] = { "." };
  return video_coding_tester_main(1, argv);
}
