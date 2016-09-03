// Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "common.h"

void OpusTests::COpusTestBase::InterchangeableVerifyResult() {
  static const wchar_t kErrorString[] = L"A fatal error was detected";
  if (m_wsOutput.find(kErrorString) != std::wstring::npos) {
    SetSucceed(false);
    m_wsResultMessage.assign(L"See test output for details.");
  }
}
