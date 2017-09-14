/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_RTP_PACKET_INCOMING_H_
#define WEBRTC_LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_RTP_PACKET_INCOMING_H_

#include "webrtc/logging/rtc_event_log/events/rtc_event.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_packet.h"

namespace webrtc {

class RtpPacketReceived;

class RtcEventRtpPacketIncoming final : public RtcEvent {
 public:
  explicit RtcEventRtpPacketIncoming(const RtpPacketReceived& packet);
  ~RtcEventRtpPacketIncoming() override;

  Type GetType() const override;

  bool IsConfigEvent() const override;

  rtp::Packet header_;          // Only the packet's header will be stored here.
  const size_t packet_length_;  // Length before stripping away all but header.
};

}  // namespace webrtc

#endif  // WEBRTC_LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_RTP_PACKET_INCOMING_H_
