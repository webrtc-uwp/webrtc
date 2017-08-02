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
  RTC_DCHECK(sink_by_mid_.empty());
  RTC_DCHECK(sink_by_ssrc_.empty());
  RTC_DCHECK(sinks_by_pt_.empty());
  RTC_DCHECK(sink_by_mid_and_rsid_.empty());
  RTC_DCHECK(sink_by_rsid_.empty());
}

bool RtpDemuxer::AddSink(const RtpDemuxerCriteria& criteria,
                         RtpPacketSinkInterface* sink) {
  RTC_DCHECK(!criteria.payload_types.empty() || !criteria.ssrcs.empty() ||
             !criteria.mid.empty() || !criteria.rsid.empty());
  RTC_DCHECK(criteria.mid.empty() || Mid::IsLegalName(criteria.mid));
  RTC_DCHECK(criteria.rsid.empty() || StreamId::IsLegalName(criteria.rsid));
  RTC_DCHECK(sink);

  // We return false intead of DCHECKing for logical conflicts with the new
  // criteria because new sinks are created according to user-specified SDP and
  // we do not want to crash due to a data validation error.
  if (CriteriaWouldConflict(criteria)) {
    return false;
  }

  if (!criteria.mid.empty()) {
    if (criteria.rsid.empty()) {
      sink_by_mid_.emplace(criteria.mid, sink);
    } else {
      sink_by_mid_and_rsid_.emplace(std::make_pair(criteria.mid, criteria.rsid),
                                    sink);
    }
  } else {
    if (!criteria.rsid.empty()) {
      sink_by_rsid_.emplace(criteria.rsid, sink);
    }
  }

  for (uint32_t ssrc : criteria.ssrcs) {
    sink_by_ssrc_.emplace(ssrc, sink);
  }

  for (uint8_t payload_type : criteria.payload_types) {
    sinks_by_pt_.emplace(payload_type, sink);
  }

  RefreshKnownMids();

  return true;
}

bool RtpDemuxer::CriteriaWouldConflict(
    const RtpDemuxerCriteria& criteria) const {
  if (!criteria.mid.empty()) {
    if (criteria.rsid.empty()) {
      // If the MID is in the known_mids_ set, then there is already a sink
      // added for this MID directly, or there is a sink already added with a
      // MID, RSID pair for our MID and some RSID.
      // Adding this criteria would cause one of these rules to be shadowed, so
      // reject this new criteria.
      if (known_mids_.find(criteria.mid) != known_mids_.end()) {
        return true;
      }
    } else {
      // If the exact rule already exists, then reject this duplicate.
      if (sink_by_mid_and_rsid_.find(std::make_pair(
              criteria.mid, criteria.rsid)) != sink_by_mid_and_rsid_.end()) {
        return true;
      }
      // If there is already a sink registered for the bare MID, then this
      // criteria will never receive any packets because they will just be
      // directed to that MID sink, so reject this new criteria.
      if (sink_by_mid_.find(criteria.mid) != sink_by_mid_.end()) {
        return true;
      }
    }
  }

  for (uint32_t ssrc : criteria.ssrcs) {
    if (sink_by_ssrc_.find(ssrc) != sink_by_ssrc_.end()) {
      return true;
    }
  }

  // TODO(steveanton): May also sanity check payload types.

  return false;
}

void RtpDemuxer::RefreshKnownMids() {
  known_mids_.clear();

  for (auto const& item : sink_by_mid_) {
    const std::string& mid = item.first;
    known_mids_.insert(mid);
  }

  for (auto const& item : sink_by_mid_and_rsid_) {
    const std::string& mid = item.first.first;
    known_mids_.insert(mid);
  }
}

bool RtpDemuxer::AddSink(uint32_t ssrc, RtpPacketSinkInterface* sink) {
  RtpDemuxerCriteria criteria;
  criteria.ssrcs = {ssrc};
  return AddSink(criteria, sink);
}

void RtpDemuxer::AddSink(const std::string& rsid,
                         RtpPacketSinkInterface* sink) {
  RtpDemuxerCriteria criteria;
  criteria.rsid = rsid;
  AddSink(criteria, sink);
}

bool RtpDemuxer::RemoveSink(const RtpPacketSinkInterface* sink) {
  RTC_DCHECK(sink);
  int num_removed = RemoveFromMapByValue(&sink_by_mid_, sink) +
                    RemoveFromMapByValue(&sink_by_ssrc_, sink) +
                    RemoveFromMultimapByValue(&sinks_by_pt_, sink) +
                    RemoveFromMapByValue(&sink_by_mid_and_rsid_, sink) +
                    RemoveFromMapByValue(&sink_by_rsid_, sink);
  RefreshKnownMids();
  return num_removed > 0;
}

bool RtpDemuxer::OnRtpPacket(const RtpPacketReceived& packet) {
  RtpPacketSinkInterface* sink = ResolveSink(packet);
  if (sink != nullptr) {
    sink->OnRtpPacket(packet);
    return true;
  }
  return false;
}

RtpPacketSinkInterface* RtpDemuxer::ResolveSink(
    const RtpPacketReceived& packet) {
  // See the BUNDLE spec for high level reference to this algorithm:
  // https://tools.ietf.org/html/draft-ietf-mmusic-sdp-bundle-negotiation-38#section-10.2

  std::string packet_mid, packet_rsid, packet_rrid;
  bool has_mid = packet.GetExtension<RtpMid>(&packet_mid);
  bool has_rsid = packet.GetExtension<RtpStreamId>(&packet_rsid);
  bool has_rrid = packet.GetExtension<RepairedRtpStreamId>(&packet_rrid);
  uint32_t ssrc = packet.Ssrc();

  // The BUNDLE spec says to drop any unknown MIDs, even if the SSRC is
  // known/latched.
  if (has_mid && known_mids_.find(packet_mid) == known_mids_.end()) {
    return nullptr;
  }

  // Cache stuff we learn about SSRCs and IDs. We need to do this even if there
  // isn't a rule/sink yet because we might add an MID/RSID rule after learning
  // an MID/RSID<->SSRC association.
  if (has_mid) {
    mid_by_ssrc_[ssrc] = packet_mid;
  }
  if (has_rsid) {
    rsid_by_ssrc_[ssrc] = packet_rsid;
  }
  if (has_rrid) {
    rsid_by_ssrc_[ssrc] = packet_rrid;
  }

  // Gather the MID/RSID from the packet's header extensions and/or remembered
  // according to the SSRC.
  const std::string* mid = ResolveMid(packet_mid, ssrc);
  const std::string* rsid = ResolveRsid(packet_rsid, packet_rrid, ssrc);

  // If MID and/or RSID is specified, prioritize that for demuxing the packet.
  // We trust these are used deliberately by senders and are more likely to be
  // correct than SSRC/payload type.
  // TODO(steveanton): According to the BUNDLE spec, new SSRC mappings are only
  //                   accepted if the packet's extended sequence number is
  //                   greater than that of the last SSRC mapping update.
  //                   https://tools.ietf.org/html/rfc7941#section-4.2.6
  if (mid != nullptr) {
    RtpPacketSinkInterface* sink_by_mid = ResolveSinkByMid(*mid, ssrc);
    if (sink_by_mid != nullptr) {
      return sink_by_mid;
    }

    // RSID is scoped to a given MID if both are included.
    if (rsid != nullptr) {
      RtpPacketSinkInterface* sink_by_mid_rsid =
          ResolveSinkByMidRsid(*mid, *rsid, ssrc);
      if (sink_by_mid_rsid != nullptr) {
        return sink_by_mid_rsid;
      }
    }
  }

  // RSID can be used without MID as long as they are unique.
  if (rsid != nullptr) {
    RtpPacketSinkInterface* sink_by_rsid = ResolveSinkByRsid(*rsid, ssrc);
    if (sink_by_rsid != nullptr) {
      return sink_by_rsid;
    }
  }

  // We trust signaled SSRC more than payload type which is likely to conflict
  // between streams.
  const auto ssrc_sink_it = sink_by_ssrc_.find(ssrc);
  if (ssrc_sink_it != sink_by_ssrc_.end()) {
    return ssrc_sink_it->second;
  }

  // Legacy senders will only signal payload type, support that as last resort.
  return ResolveSinkByPayloadType(packet.PayloadType(), ssrc);
}

const std::string* RtpDemuxer::ResolveMid(const std::string& packet_mid,
                                          uint32_t ssrc) const {
  if (!packet_mid.empty()) {
    return &packet_mid;
  }
  const auto it = mid_by_ssrc_.find(ssrc);
  if (it != mid_by_ssrc_.end()) {
    return &it->second;
  }
  return nullptr;
}

const std::string* RtpDemuxer::ResolveRsid(const std::string& packet_rsid,
                                           const std::string& packet_rrid,
                                           uint32_t ssrc) const {
  if (!packet_rsid.empty()) {
    return &packet_rsid;
  }
  if (!packet_rrid.empty()) {
    return &packet_rrid;
  }
  const auto it = rsid_by_ssrc_.find(ssrc);
  if (it != rsid_by_ssrc_.end()) {
    return &it->second;
  }
  return nullptr;
}

RtpPacketSinkInterface* RtpDemuxer::ResolveSinkByMid(const std::string& mid,
                                                     uint32_t ssrc) {
  const auto it = sink_by_mid_.find(mid);
  if (it != sink_by_mid_.end()) {
    RtpPacketSinkInterface* sink = it->second;
    bool notify = AddSsrcSinkBinding(ssrc, sink);
    if (notify) {
      NotifyObservers(
          [&](SsrcBindingObserver* obs) { obs->OnSsrcBoundToMid(mid, ssrc); });
    }
    return sink;
  }
  return nullptr;
}

RtpPacketSinkInterface* RtpDemuxer::ResolveSinkByMidRsid(
    const std::string& mid,
    const std::string& rsid,
    uint32_t ssrc) {
  const auto it = sink_by_mid_and_rsid_.find(std::make_pair(mid, rsid));
  if (it != sink_by_mid_and_rsid_.end()) {
    RtpPacketSinkInterface* sink = it->second;
    bool notify = AddSsrcSinkBinding(ssrc, sink);
    if (notify) {
      NotifyObservers([&](SsrcBindingObserver* obs) {
        obs->OnSsrcBoundToMidRsid(mid, rsid, ssrc);
      });
    }
    return sink;
  }
  return nullptr;
}

RtpPacketSinkInterface* RtpDemuxer::ResolveSinkByRsid(const std::string& rsid,
                                                      uint32_t ssrc) {
  const auto it = sink_by_rsid_.find(rsid);
  if (it != sink_by_rsid_.end()) {
    RtpPacketSinkInterface* sink = it->second;
    bool notify = AddSsrcSinkBinding(ssrc, sink);
    if (notify) {
      NotifyObservers([&](SsrcBindingObserver* obs) {
        obs->OnSsrcBoundToRsid(rsid, ssrc);
      });
    }
    return sink;
  }
  return nullptr;
}

RtpPacketSinkInterface* RtpDemuxer::ResolveSinkByPayloadType(
    uint8_t payload_type,
    uint32_t ssrc) {
  const auto range = sinks_by_pt_.equal_range(payload_type);
  if (range.first != range.second) {
    auto it = range.first;
    const auto end = range.second;
    RtpPacketSinkInterface* sink = it->second;
    if (std::next(it) == end) {
      bool notify = AddSsrcSinkBinding(ssrc, sink);
      if (notify) {
        NotifyObservers([&](SsrcBindingObserver* obs) {
          obs->OnSsrcBoundToPayloadType(payload_type, ssrc);
        });
      }
      return sink;
    }
  }
  return nullptr;
}

bool RtpDemuxer::AddSsrcSinkBinding(uint32_t ssrc,
                                    RtpPacketSinkInterface* sink) {
  auto result = sink_by_ssrc_.emplace(ssrc, sink);
  auto it = result.first;
  bool inserted = result.second;
  if (inserted) {
    return true;
  }
  if (it->second != sink) {
    it->second = sink;
    return true;
  }
  return false;
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

}  // namespace webrtc
