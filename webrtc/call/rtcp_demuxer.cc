/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/call/rtcp_demuxer.h"

#include "webrtc/call/rtcp_packet_sink_interface.h"
#include "webrtc/call/rtp_rtcp_demuxer_helper.h"
#include "webrtc/common_types.h"
#include "webrtc/rtc_base/checks.h"

namespace webrtc {

RtcpDemuxer::RtcpDemuxer() = default;

RtcpDemuxer::~RtcpDemuxer() {
  RTC_DCHECK(ssrc_sinks_.empty());
  RTC_DCHECK(rsid_sinks_.empty());
  RTC_DCHECK(mid_sinks_.empty());
  RTC_DCHECK(mid_rsid_sinks_.empty());
  RTC_DCHECK(payload_type_sinks_.empty());
  RTC_DCHECK(broadcast_sinks_.empty());
}

void RtcpDemuxer::AddSink(uint32_t sender_ssrc, RtcpPacketSinkInterface* sink) {
  AddSinkToMap(&ssrc_sinks_, sender_ssrc, sink);
}

void RtcpDemuxer::AddSink(const std::string& rsid,
                          RtcpPacketSinkInterface* sink) {
  RTC_DCHECK(StreamId::IsLegalName(rsid));
  AddSinkToMap(&rsid_sinks_, rsid, sink);
}

void RtcpDemuxer::AddMidSink(const std::string& mid,
                             RtcpPacketSinkInterface* sink) {
  RTC_DCHECK(Mid::IsLegalName(mid));
  AddSinkToMap(&mid_sinks_, mid, sink);
}

void RtcpDemuxer::AddMidRsidSink(const std::string& mid,
                                 const std::string& rsid,
                                 RtcpPacketSinkInterface* sink) {
  RTC_DCHECK(Mid::IsLegalName(mid));
  RTC_DCHECK(StreamId::IsLegalName(rsid));
  const auto key = std::make_pair(mid, rsid);
  AddSinkToMap(&mid_rsid_sinks_, key, sink);
}

void RtcpDemuxer::AddPayloadTypeSink(uint8_t payload_type,
                                     RtcpPacketSinkInterface* sink) {
  AddSinkToMap(&payload_type_sinks_, payload_type, sink);
}

void RtcpDemuxer::AddBroadcastSink(RtcpPacketSinkInterface* sink) {
  RTC_DCHECK(sink);
  RTC_DCHECK(!MultimapHasValue(ssrc_sinks_, sink));
  RTC_DCHECK(!MultimapHasValue(rsid_sinks_, sink));
  RTC_DCHECK(!MultimapHasValue(mid_sinks_, sink));
  RTC_DCHECK(!MultimapHasValue(mid_rsid_sinks_, sink));
  RTC_DCHECK(!MultimapHasValue(payload_type_sinks_, sink));
  RTC_DCHECK(!ContainerHasKey(broadcast_sinks_, sink));
  broadcast_sinks_.push_back(sink);
}

void RtcpDemuxer::RemoveSink(const RtcpPacketSinkInterface* sink) {
  RTC_DCHECK(sink);
  size_t removal_count = RemoveFromMultimapByValue(&ssrc_sinks_, sink) +
                         RemoveFromMultimapByValue(&rsid_sinks_, sink) +
                         RemoveFromMultimapByValue(&mid_sinks_, sink) +
                         RemoveFromMultimapByValue(&mid_rsid_sinks_, sink) +
                         RemoveFromMultimapByValue(&payload_type_sinks_, sink);
  RTC_DCHECK_GT(removal_count, 0);
}

void RtcpDemuxer::RemoveBroadcastSink(const RtcpPacketSinkInterface* sink) {
  RTC_DCHECK(sink);
  auto it = std::find(broadcast_sinks_.begin(), broadcast_sinks_.end(), sink);
  RTC_DCHECK(it != broadcast_sinks_.end());
  broadcast_sinks_.erase(it);
}

void RtcpDemuxer::OnRtcpPacket(rtc::ArrayView<const uint8_t> packet) {
  // Perform sender-SSRC-based demuxing for packets with a sender-SSRC.
  rtc::Optional<uint32_t> sender_ssrc = ParseRtcpPacketSenderSsrc(packet);
  if (sender_ssrc) {
    auto it_range = ssrc_sinks_.equal_range(*sender_ssrc);
    for (auto it = it_range.first; it != it_range.second; ++it) {
      it->second->OnRtcpPacket(packet);
    }
  }

  // All packets, even those without a sender-SSRC, are broadcast to sinks
  // which listen to broadcasts.
  for (RtcpPacketSinkInterface* sink : broadcast_sinks_) {
    sink->OnRtcpPacket(packet);
  }
}

void RtcpDemuxer::OnSsrcBoundToRsid(const std::string& rsid, uint32_t ssrc) {
  BindSinksToSsrc(rsid_sinks_, rsid, ssrc);
}

void RtcpDemuxer::OnSsrcBoundToMid(const std::string& mid, uint32_t ssrc) {
  BindSinksToSsrc(mid_sinks_, mid, ssrc);
}

void RtcpDemuxer::OnSsrcBoundToMidRsid(const std::string& mid,
                                       const std::string& rsid,
                                       uint32_t ssrc) {
  BindSinksToSsrc(mid_rsid_sinks_, std::make_pair(mid, rsid), ssrc);
}

void RtcpDemuxer::OnSsrcBoundToPayloadType(uint8_t payload_type,
                                           uint32_t ssrc) {
  BindSinksToSsrc(payload_type_sinks_, payload_type, ssrc);
}

}  // namespace webrtc
