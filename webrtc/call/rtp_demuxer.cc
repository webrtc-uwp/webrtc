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
RtpDemuxerCriteria::RtpDemuxerCriteria(const RtpDemuxerCriteria& o) = default;
RtpDemuxerCriteria::~RtpDemuxerCriteria() = default;

RtpDemuxer::RtpDemuxer() = default;

RtpDemuxer::~RtpDemuxer() {
  RTC_DCHECK(sinks_.empty());
  RTC_DCHECK(ssrc_sink_mapping_.empty());
}

void RtpDemuxer::AddSink(RtpDemuxerCriteria criteria,
                         RtpPacketSinkInterface* sink) {
  RTC_DCHECK(!criteria.payload_types.size() || !criteria.ssrcs.empty() ||
             criteria.mid || criteria.rsid);
  if (criteria.mid) {
    RTC_DCHECK(Mid::IsLegalName(*criteria.mid));
  }
  if (criteria.rsid) {
    RTC_DCHECK(StreamId::IsLegalName(*criteria.rsid));
  }
  RTC_DCHECK(sink);

  for (uint32_t ssrc : criteria.ssrcs) {
    AddSsrcSinkBinding(ssrc, sink);
  }

  sinks_.emplace_back(std::move(criteria), sink);
}

bool RtpDemuxer::AddSsrcSinkBinding(uint32_t ssrc,
                                    RtpPacketSinkInterface* sink) {
  RTC_DCHECK(sink);

  auto result = ssrc_sink_mapping_.emplace(ssrc, sink);
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
  RtpDemuxerCriteria criteria;
  criteria.ssrcs.push_back(ssrc);
  RTC_DCHECK(sink);
  // The association might already have been set by a different
  // configuration source.
  // We cannot RTC_DCHECK against an attempt to remap an SSRC, because
  // such a configuration might have come from the network (1. resolution
  // of an RSID or 2. RTCP messages with RSID resolutions).
  // return ssrc_sinks_.emplace(ssrc, sink).second;

  return true;
}

void RtpDemuxer::AddSink(const std::string& rsid,
                         RtpPacketSinkInterface* sink) {
  RtpDemuxerCriteria criteria;
  criteria.rsid.emplace(rsid);
  AddSink(std::move(criteria), sink);
}

bool RtpDemuxer::RemoveSink(const RtpPacketSinkInterface* sink) {
  RTC_DCHECK(sink);
  return (RemoveFromMapByValue(&sinks_, sink) +
          RemoveFromMapByValue(&ssrc_sink_mapping_, sink)) > 0;
}

bool RtpDemuxer::OnRtpPacket(const RtpPacketReceived& packet) {
  // Fast path. If we don't have an SSRC mapping, one of the following should
  // find the appropriate sink and create an SSRC mapping so that next time we
  // can just return here.
  if (TryDemuxWithSsrc(packet)) {
    return true;
  }

  // Needed for legacy endpoints that don't signal SSRCs, MIDs, or RSIDs.
  if (TryDemuxWithPayloadType(packet)) {
    return true;
  }

  // Needed for standard WebRTC endpoints that don't signal SSRCs.

  // Can work when MIDs are unique.
  if (TryDemuxWithMid(packet)) {
    return true;
  }

  // Can work when RSIDs are unique.
  if (TryDemuxWithRsid(packet)) {
    return true;
  }

  // Can work if RSID is not unique and must be scoped by MID.
  if (TryDemuxWithMidRsid(packet)) {
    return true;
  }

  return false;
}

bool RtpDemuxer::TryDemuxWithSsrc(const RtpPacketReceived& packet) {
  auto it = ssrc_sink_mapping_.find(packet.Ssrc());
  if (it != ssrc_sink_mapping_.end()) {
    RtpPacketSinkInterface* sink = it->second;
    sink->OnRtpPacket(packet);
    return true;
  }
  return false;
}

bool RtpDemuxer::TryDemuxWithPayloadType(const RtpPacketReceived& packet) {
  RtpPacketSinkInterface* sink =
      FindSinkByPredicate([&](const RtpDemuxerCriteria& criteria) -> bool {
        const std::vector<uint8_t>& payload_types = criteria.payload_types;
        return std::find(payload_types.begin(), payload_types.end(),
                         packet.PayloadType()) != payload_types.end();
      });
  if (sink != nullptr) {
    LOG_F(LS_INFO) << "Resolving payload type " << packet.PayloadType()
                   << " to SSRC " << packet.Ssrc();
    AddSsrcSinkBinding(packet.Ssrc(), sink);
    sink->OnRtpPacket(packet);
    return true;
  }
  return false;
}

bool RtpDemuxer::TryDemuxWithMid(const RtpPacketReceived& packet) {
  std::string mid;
  if (!packet.GetExtension<RtpMid>(&mid)) {
    return false;
  }
  RtpPacketSinkInterface* sink =
      FindSinkByPredicate([&](const RtpDemuxerCriteria& criteria) -> bool {
        return criteria.mid && *criteria.mid == mid;
      });
  if (sink != nullptr) {
    LOG_F(LS_INFO) << "Resolving MID " << mid << " to SSRC " << packet.Ssrc();
    // TODO(steveanton): Notify?
    // NotifyObserversOfMidResolution(mid, packet.Ssrc());
    AddSsrcSinkBinding(packet.Ssrc(), sink);
    sink->OnRtpPacket(packet);
    return true;
  }
  return false;
}

bool RtpDemuxer::TryDemuxWithRsid(const RtpPacketReceived& packet) {
  std::string rsid;
  if (!packet.GetExtension<RtpStreamId>(&rsid)) {
    return false;
  }
  RtpPacketSinkInterface* sink =
      FindSinkByPredicate([&](const RtpDemuxerCriteria& criteria) -> bool {
        return criteria.rsid && *criteria.rsid == rsid;
      });
  if (sink != nullptr) {
    LOG_F(LS_INFO) << "Resolving RSID " << rsid << " to SSRC " << packet.Ssrc();
    NotifyObserversOfRsidResolution(rsid, packet.Ssrc());
    AddSsrcSinkBinding(packet.Ssrc(), sink);
    sink->OnRtpPacket(packet);
    return true;
  }
  return false;
}

bool RtpDemuxer::TryDemuxWithMidRsid(const RtpPacketReceived& packet) {
  std::string mid;
  if (!packet.GetExtension<RtpMid>(&mid)) {
    return false;
  }
  std::string rsid;
  if (!packet.GetExtension<RtpStreamId>(&rsid)) {
    return false;
  }
  RtpPacketSinkInterface* sink =
      FindSinkByPredicate([&](const RtpDemuxerCriteria& criteria) -> bool {
        return criteria.mid && criteria.rsid && *criteria.mid == mid &&
               *criteria.rsid == rsid;
      });
  if (sink != nullptr) {
    LOG_F(LS_INFO) << "Resolving MID, RSID pair " << mid << "," << rsid
                   << " to SSRC " << packet.Ssrc();
    // TODO(steveanton): Notify?
    AddSsrcSinkBinding(packet.Ssrc(), sink);
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
