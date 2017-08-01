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

  ResetKnownMids();

  return true;
}

bool RtpDemuxer::CriteriaWouldConflict(
    const RtpDemuxerCriteria& criteria) const {
  if (!criteria.mid.empty()) {
    if (criteria.rsid.empty()) {
      if (sink_by_mid_.find(criteria.mid) != sink_by_mid_.end()) {
        return true;
      }
    } else {
      if (sink_by_mid_and_rsid_.find(std::make_pair(
              criteria.mid, criteria.rsid)) != sink_by_mid_and_rsid_.end()) {
        return true;
      }
    }
  }

  for (uint32_t ssrc : criteria.ssrcs) {
    if (sink_by_ssrc_.find(ssrc) != sink_by_ssrc_.end()) {
      return true;
    }
  }

  return false;
}

void RtpDemuxer::ResetKnownMids() {
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
  ResetKnownMids();
  return num_removed > 0;
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
  std::string p_mid, p_rsid, p_rrid;
  bool has_mid = packet.GetExtension<RtpMid>(&p_mid);
  bool has_rsid = packet.GetExtension<RtpStreamId>(&p_rsid);
  bool has_rrid = packet.GetExtension<RepairedRtpStreamId>(&p_rrid);
  uint32_t ssrc = packet.Ssrc();

  // The BUNDLE spec says to drop any unknown MIDs, even if the SSRC is
  // known/latched.
  if (has_mid && known_mids_.find(p_mid) == known_mids_.end()) {
    return nullptr;
  }

  // Cache stuff we learn about SSRCs and IDs. We need to do this even if there
  // isn't a rule/sink yet because we might add an MID/RSID rule after learning
  // an MID/RSID<->SSRC association.
  if (has_mid) {
    mid_by_ssrc_[ssrc] = p_mid;
  }
  if (has_rsid) {
    rsid_by_ssrc_[ssrc] = p_rsid;
  }
  if (has_rrid) {
    rsid_by_ssrc_[ssrc] = p_rrid;
  }

  // Run the BUNDLE spec rules.
  const std::string mid = ResolveMid(p_mid, ssrc);
  const std::string rsid = ResolveRsid(p_rsid, p_rrid, ssrc);

  if (!mid.empty()) {
    RtpPacketSinkInterface* sink_by_mid = FindSinkByMid(mid, ssrc);
    if (sink_by_mid != nullptr) {
      return sink_by_mid;
    }

    if (!rsid.empty()) {
      RtpPacketSinkInterface* sink_by_mid_rsid =
          FindSinkByMidRsid(mid, rsid, ssrc);
      if (sink_by_mid_rsid != nullptr) {
        return sink_by_mid_rsid;
      }
    }
  }

  if (!rsid.empty()) {
    RtpPacketSinkInterface* sink_by_rsid = FindSinkByRsid(rsid, ssrc);
    if (sink_by_rsid != nullptr) {
      return sink_by_rsid;
    }
  }

  const auto ssrc_sink_it = sink_by_ssrc_.find(ssrc);
  if (ssrc_sink_it != sink_by_ssrc_.end()) {
    return ssrc_sink_it->second;
  }

  return FindSinkByPayloadType(packet.PayloadType(), ssrc);
}

std::string RtpDemuxer::ResolveMid(const std::string& p_mid,
                                   uint32_t ssrc) const {
  if (!p_mid.empty()) {
    return p_mid;
  }
  const auto it = mid_by_ssrc_.find(ssrc);
  if (it != mid_by_ssrc_.end()) {
    return it->second;
  }
  return "";
}

std::string RtpDemuxer::ResolveRsid(const std::string& p_rsid,
                                    const std::string& p_rrid,
                                    uint32_t ssrc) const {
  if (!p_rsid.empty()) {
    return p_rsid;
  }
  if (!p_rrid.empty()) {
    return p_rrid;
  }
  const auto it = rsid_by_ssrc_.find(ssrc);
  if (it != rsid_by_ssrc_.end()) {
    return it->second;
  }
  return "";
}

RtpPacketSinkInterface* RtpDemuxer::FindSinkByMid(const std::string& mid,
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

RtpPacketSinkInterface* RtpDemuxer::FindSinkByMidRsid(const std::string& mid,
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

RtpPacketSinkInterface* RtpDemuxer::FindSinkByRsid(const std::string& rsid,
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

RtpPacketSinkInterface* RtpDemuxer::FindSinkByPayloadType(uint8_t payload_type,
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
