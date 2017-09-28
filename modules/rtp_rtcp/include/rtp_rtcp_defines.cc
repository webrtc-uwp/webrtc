/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"

namespace webrtc {

PayloadUnion::PayloadUnion(const AudioPayload& payload)
    : Audio(payload), is_audio_(true) {}
PayloadUnion::PayloadUnion(const VideoPayload& payload)
    : Video(payload), is_audio_(false) {}
PayloadUnion::PayloadUnion(const PayloadUnion&) = default;
PayloadUnion::PayloadUnion(PayloadUnion&&) = default;
PayloadUnion::~PayloadUnion() = default;

PayloadUnion& PayloadUnion::operator=(const PayloadUnion&) = default;
PayloadUnion& PayloadUnion::operator=(PayloadUnion&&) = default;
void swap(PayloadUnion& a, PayloadUnion& b) {
  using std::swap;
  swap(a.Audio, b.Audio);
  swap(a.Video, b.Video);
  swap(a.is_audio_, b.is_audio_);
}

}  // namespace webrtc
