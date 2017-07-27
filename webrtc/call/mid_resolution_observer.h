/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_CALL_MID_RESOLUTION_OBSERVER_H_
#define WEBRTC_CALL_MID_RESOLUTION_OBSERVER_H_

#include <string>

#include "webrtc/rtc_base/basictypes.h"

namespace webrtc {

// Any number of SSRCs can be associated with a MID throughout a call. We learn
// these associations as RTP packets with the MID extension header are received.
class MidResolutionObserver {
 public:
  virtual ~MidResolutionObserver() = default;

  // Called when an SSRC is associated with a MID.
  virtual void OnMidResolved(const std::string& mid, uint32_t ssrc) = 0;
};

}  // namespace webrtc

#endif  // WEBRTC_CALL_MID_RESOLUTION_OBSERVER_H_
