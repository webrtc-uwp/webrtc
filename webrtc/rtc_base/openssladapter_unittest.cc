/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/rtc_base/gunit.h"
#include "webrtc/rtc_base/openssladapter.h"

namespace rtc {

TEST(OpenSSLAdapterTest, TestTransformAlpnProtocols) {
  EXPECT_EQ(TransformAlpnProtocolsForTesting(""), "");

  std::string large_protocol(256, 'a');
  EXPECT_EQ(TransformAlpnProtocolsForTesting(large_protocol), "");

  std::string one_proto = "h2";
  std::stringstream expected_response;
  expected_response << static_cast<char>(2) << "h2";
  EXPECT_EQ(TransformAlpnProtocolsForTesting(one_proto),
            expected_response.str());

  std::string standard_protos = "h2,http/1.1";
  expected_response << static_cast<char>(8) << "http/1.1";
  EXPECT_EQ(TransformAlpnProtocolsForTesting(standard_protos),
            expected_response.str());

  std::string invalid_formatting1 = ",a";
  std::string invalid_formatting2 = "a,";
  EXPECT_EQ(TransformAlpnProtocolsForTesting(invalid_formatting1), "");
  EXPECT_EQ(TransformAlpnProtocolsForTesting(invalid_formatting2), "");
}

}  // namespace rtc

