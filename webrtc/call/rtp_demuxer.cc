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

#include "webrtc/call/mid_resolution_observer.h"
#include "webrtc/call/rsid_resolution_observer.h"
#include "webrtc/call/rtp_packet_sink_interface.h"
#include "webrtc/call/rtp_rtcp_demuxer_helper.h"
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
  RTC_DCHECK(sinks_by_rsid_.empty());
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

  for (uint32_t ssrc : criteria.ssrcs) {
    if (GetSinkBySsrc(ssrc) != nullptr) {
      return false;
    }
  }

  if (!criteria.mid.empty()) {
    sinks_by_mid_.emplace(criteria.mid, sink);
  }

  for (uint32_t ssrc : criteria.ssrcs) {
    bool inserted = AddSsrcSinkBinding(ssrc, sink);
    RTC_DCHECK(inserted);
  }

  for (uint8_t payload_type : criteria.payload_types) {
    sinks_by_payload_type_.emplace(payload_type, sink);
  }

  for (const std::string& rsid : criteria.rsids) {
    sinks_by_rsid_.emplace(rsid, sink);
    if (!criteria.mid.empty()) {
      sinks_by_mid_rsid_.emplace(std::make_pair(criteria.mid, rsid), sink);
    }
  }

  return true;
}

bool RtpDemuxer::AddSsrcSinkBinding(uint32_t ssrc,
                                    RtpPacketSinkInterface* sink) {
  RTC_DCHECK(sink);

  auto result = sinks_by_ssrc_.emplace(ssrc, sink);
  bool inserted = result.second;
  if (!inserted) {
    RtpPacketSinkInterface* existing_sink = result.first->second;
    if (sink != existing_sink) {
      return false;
    }
  }
  return true;
}

bool RtpDemuxer::AddSink(uint32_t ssrc, RtpPacketSinkInterface* sink) {
  // The association might already have been set by a different
  // configuration source.
  // We cannot RTC_DCHECK against an attempt to remap an SSRC, because
  // such a configuration might have come from the network (1. resolution
  // of an RSID or 2. RTCP messages with RSID resolutions).
  RtpDemuxerCriteria criteria;
  criteria.ssrcs.emplace_back(ssrc);
  return AddSink(criteria, sink);
}

void RtpDemuxer::AddSink(const std::string& rsid,
                         RtpPacketSinkInterface* sink) {
  RtpDemuxerCriteria criteria;
  criteria.rsids.emplace_back(rsid);
  AddSink(criteria, sink);
}

bool RtpDemuxer::RemoveSink(const RtpPacketSinkInterface* sink) {
  RTC_DCHECK(sink);
  return (RemoveFromMultimapByValue(&sinks_by_mid_, sink) +
          RemoveFromMapByValue(&sinks_by_ssrc_, sink) +
          RemoveFromMultimapByValue(&sinks_by_payload_type_, sink) +
          RemoveFromMultimapByValue(&sinks_by_rsid_, sink) +
          RemoveFromMultimapByValue(&sinks_by_mid_rsid_, sink)) > 0;
}

template <typename T>
RtpPacketSinkInterface* FindSinkInMap(
    const std::multimap<T, RtpPacketSinkInterface*> map,
    const T& key) {
  auto it = map.equal_range(key);
  auto it_front = it.first;
  auto it_back = it.second;
  if (it_front != it_back) {
    // At least one element
    RtpPacketSinkInterface* sink = it_front->second;
    if (++it_front != it_back) {
      // More than one element: ambiguous
      return nullptr;
    }
    return sink;
  }
  return nullptr;
}

RtpPacketSinkInterface* RtpDemuxer::GetSinkBySsrc(uint32_t ssrc) const {
  auto it = sinks_by_ssrc_.find(ssrc);
  if (it != sinks_by_ssrc_.end()) {
    return it->second;
  }
  return nullptr;
}

RtpPacketSinkInterface* RtpDemuxer::FindSink(const RtpPacketReceived& packet) {
  std::string mid;
  bool has_mid = packet.GetExtension<RtpMid>(&mid);
  if (has_mid) {
    auto it = sinks_by_mid_.equal_range(mid);
    auto range_front = it.first;
    auto range_back = it.second;
    if (range_front == range_back) {
      // Unknown MID
      return nullptr;
    }
    RtpPacketSinkInterface* mid_sink = range_front->second;
    if (++range_front == range_back) {
      // Exactly one sink registered for this MID
      LOG_F(LS_INFO) << "Resolving MID " << mid << " to SSRC " << packet.Ssrc();
      bool added = AddSsrcSinkBinding(packet.Ssrc(), mid_sink);
      if (added) {
        return mid_sink;
      } else {
        return GetSinkBySsrc(packet.Ssrc());
      }
    }
  }

  // Lookup by SSRC
  RtpPacketSinkInterface* ssrc_sink = GetSinkBySsrc(packet.Ssrc());
  if (ssrc_sink != nullptr) {
    return ssrc_sink;
  }

  // Lookup by payload_type
  RtpPacketSinkInterface* payload_type_sink =
      FindSinkInMap(sinks_by_payload_type_, packet.PayloadType());
  if (payload_type_sink != nullptr) {
    LOG_F(LS_INFO) << "Resolving payload type " << packet.PayloadType()
                   << " to SSRC " << packet.Ssrc();
    AddSsrcSinkBinding(packet.Ssrc(), payload_type_sink);
    return payload_type_sink;
  }

  // Lookup by RSID
  std::string rsid;
  bool has_rsid = packet.GetExtension<RtpStreamId>(&rsid);
  if (has_rsid) {
    RtpPacketSinkInterface* rsid_sink = FindSinkInMap(sinks_by_rsid_, rsid);
    if (rsid_sink != nullptr) {
      LOG_F(LS_INFO) << "Resolving RSID " << rsid << " to SSRC "
                     << packet.Ssrc();
      AddSsrcSinkBinding(packet.Ssrc(), rsid_sink);
      NotifyObserversOfRsidResolution(rsid, packet.Ssrc());
      return rsid_sink;
    }
  }

  // Lookup by MID, RSID
  if (has_mid && has_rsid) {
    RtpPacketSinkInterface* mid_rsid_sink =
        FindSinkInMap(sinks_by_mid_rsid_, std::make_pair(mid, rsid));
    if (mid_rsid_sink != nullptr) {
      LOG_F(LS_INFO) << "Resolving MID, RSID pair " << mid << "," << rsid
                     << " to SSRC " << packet.Ssrc();
      AddSsrcSinkBinding(packet.Ssrc(), mid_rsid_sink);
      return mid_rsid_sink;
    }
  }

  // No match.
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

void RtpDemuxer::RegisterRsidResolutionObserver(
    RsidResolutionObserver* observer) {
  RTC_DCHECK(observer);
  RTC_DCHECK(!ContainerHasKey(rsid_resolution_observers_, observer));

  rsid_resolution_observers_.push_back(observer);
}

void RtpDemuxer::DeregisterRsidResolutionObserver(
    const RsidResolutionObserver* observer) {
  RTC_DCHECK(observer);
  auto it = std::find(rsid_resolution_observers_.begin(),
                      rsid_resolution_observers_.end(), observer);
  RTC_DCHECK(it != rsid_resolution_observers_.end());
  rsid_resolution_observers_.erase(it);
}

void RtpDemuxer::NotifyObserversOfRsidResolution(const std::string& rsid,
                                                 uint32_t ssrc) {
  for (auto* observer : rsid_resolution_observers_) {
    observer->OnRsidResolved(rsid, ssrc);
  }
}

}  // namespace webrtc
