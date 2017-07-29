/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/call/rtp_demuxer.h"

#include "webrtc/call/rtp_packet_sink_interface.h"
#include "webrtc/call/rtp_rtcp_demuxer_helper.h"
#include "webrtc/call/ssrc_binding_observer.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_packet_received.h"
#include "webrtc/rtc_base/checks.h"
#include "webrtc/rtc_base/logging.h"

namespace webrtc {

RtpDemuxerCriteria::RtpDemuxerCriteria() = default;
RtpDemuxerCriteria::~RtpDemuxerCriteria() = default;

RtpDemuxer::RtpDemuxer() = default;

RtpDemuxer::~RtpDemuxer() {
  RTC_DCHECK(sinks_by_mid_.empty());
  RTC_DCHECK(sinks_by_ssrc_.empty());
  RTC_DCHECK(sinks_by_payload_type_.empty());
  RTC_DCHECK(sinks_by_mid_rsid_.empty());
}

bool RtpDemuxer::AddSink(const RtpDemuxerCriteria& criteria,
                         RtpPacketSinkInterface* sink) {
  RTC_DCHECK(!criteria.payload_types.empty() || !criteria.ssrcs.empty() ||
             !criteria.mid.empty() || !criteria.rsids.empty());
  RTC_DCHECK(criteria.mid.empty() || Mid::IsLegalName(criteria.mid));
  RTC_DCHECK(std::all_of(criteria.rsids.begin(), criteria.rsids.end(),
                         StreamId::IsLegalName));
  RTC_DCHECK(sink);

  if (CriteriaWouldConflict(criteria)) {
    return false;
  }

  if (!criteria.mid.empty()) {
    sinks_by_mid_.emplace(criteria.mid, sink);
  }

  for (uint32_t ssrc : criteria.ssrcs) {
    sinks_by_ssrc_[ssrc] = sink;
  }

  for (uint8_t payload_type : criteria.payload_types) {
    sinks_by_payload_type_.emplace(payload_type, sink);
  }

  for (const std::string& rsid : criteria.rsids) {
    sinks_by_mid_rsid_.emplace(std::make_pair(criteria.mid, rsid), sink);
  }

  return true;
}

bool RtpDemuxer::CriteriaWouldConflict(
    const RtpDemuxerCriteria& criteria) const {
  for (uint32_t ssrc : criteria.ssrcs) {
    if (GetSinkBySsrc(ssrc) != nullptr) {
      return true;
    }
  }

  for (const std::string& rsid : criteria.rsids) {
    const auto key = std::make_pair(criteria.mid, rsid);
    if (sinks_by_mid_rsid_.find(key) != sinks_by_mid_rsid_.end()) {
      return true;
    }
  }

  return false;
}

bool RtpDemuxer::AddSink(uint32_t ssrc, RtpPacketSinkInterface* sink) {
  // The association might already have been set by a different
  // configuration source.
  // We cannot RTC_DCHECK against an attempt to remap an SSRC, because
  // such a configuration might have come from the network (1. resolution
  // of an RSID or 2. RTCP messages with RSID resolutions).
  RtpDemuxerCriteria criteria;
  criteria.ssrcs = {ssrc};
  return AddSink(criteria, sink);
}

void RtpDemuxer::AddSink(const std::string& rsid,
                         RtpPacketSinkInterface* sink) {
  RtpDemuxerCriteria criteria;
  criteria.rsids = {rsid};
  AddSink(criteria, sink);
}

bool RtpDemuxer::RemoveSink(const RtpPacketSinkInterface* sink) {
  RTC_DCHECK(sink);
  return (RemoveFromMultimapByValue(&sinks_by_mid_, sink) +
          RemoveFromMapByValue(&sinks_by_ssrc_, sink) +
          RemoveFromMultimapByValue(&sinks_by_payload_type_, sink) +
          RemoveFromMapByValue(&sinks_by_mid_rsid_, sink)) > 0;
}

RtpPacketSinkInterface* RtpDemuxer::GetSinkBySsrc(uint32_t ssrc) const {
  const auto it = sinks_by_ssrc_.find(ssrc);
  if (it != sinks_by_ssrc_.end()) {
    return it->second;
  }
  return nullptr;
}

bool RtpDemuxer::OnRtpPacket(const RtpPacketReceived& packet) {
  RtpPacketSinkInterface* sink = FindSink(packet);
  if (sink != nullptr) {
    sink->OnRtpPacket(packet);
    return true;
  }
  return false;
}

RtpPacketSinkInterface* RtpDemuxer::FindSink(const RtpPacketReceived& packet) {
  // Try to find the appropriate sink by searching according to the order
  // specified in the BUNDLE spec.

  const auto result = FindSinkByMid(packet);
  RtpPacketSinkInterface* sink = result.first;
  const bool drop_packet = result.second;
  if (drop_packet) {
    return nullptr;
  }
  if (sink != nullptr) {
    return sink;
  }

  sink = GetSinkBySsrc(packet.Ssrc());
  if (sink != nullptr) {
    return sink;
  }

  sink = FindSinkByPayloadType(packet);
  if (sink != nullptr) {
    return sink;
  }

  sink = FindSinkByRsid(packet);
  if (sink != nullptr) {
    return sink;
  }

  return nullptr;
}

std::pair<RtpPacketSinkInterface*, bool> RtpDemuxer::FindSinkByMid(
    const RtpPacketReceived& packet) {
  std::string mid;
  if (packet.GetExtension<RtpMid>(&mid)) {
    const auto it = sinks_by_mid_.equal_range(mid);
    const auto range_begin = it.first;
    const auto range_end = it.second;
    if (range_begin != range_end) {
      // At least one sink registered for this MID.
      RtpPacketSinkInterface* sink = range_begin->second;
      if (std::next(range_begin) == range_end) {
        // Exactly one sink registered for this MID.
        LOG_F(LS_INFO) << "Resolving MID " << mid << " to SSRC "
                       << packet.Ssrc() << ".";
        sinks_by_ssrc_[packet.Ssrc()] = sink;
        return std::make_pair(sink, false);
      }
      // Otherwise, ambiguous which sink to route to so defer to later stages.
    } else {
      // According to BUNDLE, if the packet specifies a MID that does not have
      // a mapping in the table it should be dropped.
      return std::make_pair(nullptr, true);
    }
  }
  return std::make_pair(nullptr, false);
}

RtpPacketSinkInterface* RtpDemuxer::FindSinkByPayloadType(
    const RtpPacketReceived& packet) {
  const auto it = sinks_by_payload_type_.equal_range(packet.PayloadType());
  const auto range_begin = it.first;
  const auto range_end = it.second;
  if (range_begin != range_end) {
    // At least one sink registered for this payload type.
    RtpPacketSinkInterface* sink = range_begin->second;
    if (std::next(range_begin) == range_end) {
      // Exactly one sink registered for this payload type.
      LOG_F(LS_INFO) << "Resolving payload type " << packet.PayloadType()
                     << " to SSRC " << packet.Ssrc() << ".";
      sinks_by_ssrc_[packet.Ssrc()] = sink;
      return sink;
    }
    // Otherwise, ambiguous which sink to route to so defer to later stages.
  }
  return nullptr;
}

RtpPacketSinkInterface* RtpDemuxer::FindSinkByRsid(
    const RtpPacketReceived& packet) {
  // RSIDs are scoped within MIDs, so we need to look them up together. For
  // example, it is legal to have two separate streams with one MID=A, RSID=1
  // and the other MID=B, RSID=1. See discussion here:
  // https://tools.ietf.org/html/draft-ietf-avtext-rid-09#section-3
  std::string rsid;
  if (packet.GetExtension<RtpStreamId>(&rsid)) {
    std::string mid;
    if (!packet.GetExtension<RtpMid>(&mid)) {
      // If no MID specified, use the empty string as sentinel value.
      mid = "";
    }
    const auto it = sinks_by_mid_rsid_.find(std::make_pair(mid, rsid));
    if (it != sinks_by_mid_rsid_.end()) {
      RtpPacketSinkInterface* sink = it->second;
      if (!mid.empty()) {
        LOG_F(LS_INFO) << "Resolving MID,RSID pair " << mid << "," << rsid
                       << " to SSRC " << packet.Ssrc() << ".";
      } else {
        LOG_F(LS_INFO) << "Resolving RSID " << rsid << " to SSRC "
                       << packet.Ssrc() << ".";
      }
      sinks_by_ssrc_[packet.Ssrc()] = sink;
      NotifyObserversOfRsidResolution(rsid, packet.Ssrc());
      return sink;
    }
  }
  return nullptr;
}

void RtpDemuxer::RegisterSsrcBindingObserver(SsrcBindingObserver* observer) {
  RTC_DCHECK(observer);
  RTC_DCHECK(!ContainerHasKey(ssrc_binding_observers_, observer));

  ssrc_binding_observers_.push_back(observer);
}

void RtpDemuxer::DeregisterSsrcBindingObserver(
    const SsrcBindingObserver* observer) {
  RTC_DCHECK(observer);
  auto it = std::find(ssrc_binding_observers_.begin(),
                      ssrc_binding_observers_.end(), observer);
  RTC_DCHECK(it != ssrc_binding_observers_.end());
  ssrc_binding_observers_.erase(it);
}

void RtpDemuxer::NotifyObserversOfRsidResolution(const std::string& rsid,
                                                 uint32_t ssrc) {
  for (auto* observer : ssrc_binding_observers_) {
    observer->OnBindingFromRsid(rsid, ssrc);
  }
}

}  // namespace webrtc
