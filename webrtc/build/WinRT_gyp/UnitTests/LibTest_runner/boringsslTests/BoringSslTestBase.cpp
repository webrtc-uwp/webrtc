
// Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "webrtc/build/WinRT_gyp/UnitTests/LibTest_runner/common.h"

namespace BoringSSLTests {
  void CBoringSSLTestBase::InterchangeableVerifyResult() {
    static const wchar_t* kPassKyeword = L"PASS";
    SetSucceed(m_wsOutput.find(kPassKyeword) != std::wstring::npos);
  }
}  // BoringSSLTests

