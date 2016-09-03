// Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "common.h"
#include "SrtpAesCalcTest.h"

// test entry point declaration
extern "C" int srtp_test_aes_calc_main(int argc, char *argv[]);

AUTO_ADD_TEST_IMPL(libSrtpTests::CSrtpAesCalcTest);

// following values are from aec_calc.c
#define KEY "000102030405060708090a0b0c0d0e0f"
#define PLAIN_TEXT "00112233445566778899aabbccddeeff"
// output
#define CIPHER_TEXT L"69c4e0d86a7b0430d8cdb78070b4c55a"

int libSrtpTests::CSrtpAesCalcTest::InterchangeableExecute() {
  char* argv[] = { ".", KEY, PLAIN_TEXT, "-v" };

  return srtp_test_aes_calc_main(4, argv);
}

void libSrtpTests::CSrtpAesCalcTest::InterchangeableVerifyResult() {
  static const wchar_t kCipherTextKey[] = L"ciphertext";
  __super::InterchangeableVerifyResult();

  if (Succeed()) {
    size_t begin = m_wsOutput.find_first_not_of(L" :\t",
                      Output().rfind(kCipherTextKey) + wcslen(kCipherTextKey));
    size_t end = m_wsOutput.find_first_of(L" \n", begin);
    std::wstring cipherText(m_wsOutput.cbegin() + begin,
                            m_wsOutput.cbegin() + end);

    if (cipherText.compare(CIPHER_TEXT) != 0) {
      SetSucceed(false);
      m_wsResultMessage +=
        L"ciphertext doesn't match. Expected: " CIPHER_TEXT L"\n";
    }
  }
}
