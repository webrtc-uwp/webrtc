/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_CALL_RTCP_DEMUXER_H_
#define WEBRTC_CALL_RTCP_DEMUXER_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "webrtc/call/rtp_rtcp_demuxer_helper.h"
#include "webrtc/call/ssrc_binding_observer.h"
#include "webrtc/rtc_base/array_view.h"
#include "webrtc/rtc_base/basictypes.h"

namespace webrtc {

class RtcpPacketSinkInterface;

// This class represents the RTCP demuxing, for a single RTP session (i.e., one
// SSRC space, see RFC 7656). It isn't thread aware, leaving responsibility of
// multithreading issues to the user of this class.
//
// Notes for adding sinks:
// - Sinks may not be null.
// - For each of SSRC, RSID, MID, and payload type, there can be multiple
//   distinct sinks added for a particular value, and the same sink can be added
//   in any number of distinct ways, but the same sink cannot be added multiple
//   times for the same value.
// - A sink can either be added as specific for a type or as broadcast, but not
//   both.
class RtcpDemuxer : public SsrcBindingObserver {
 public:
  RtcpDemuxer();
  ~RtcpDemuxer() override;

  // Registers a sink. The sink will be notified of incoming RTCP packets with
  // that sender-SSRC.
  // TODO(steveanton): Rename this to AddSsrcSink
  void AddSink(uint32_t sender_ssrc, RtcpPacketSinkInterface* sink);

  // Registers a sink. Once the RSID is resolved to an SSRC, the sink will be
  // notified of all RTCP packets with that sender-SSRC.
  // TODO(steveanton): Rename this to AddRsidSink
  void AddSink(const std::string& rsid, RtcpPacketSinkInterface* sink);

  // Registers a sink. Once an SSRC is bound to the MID, the sink will be
  // notified of all RTCP packets with any of those sender-SSRCs.
  void AddMidSink(const std::string& mid, RtcpPacketSinkInterface* sink);

  // Registers a sink. Once an SSRC is bound to the MID,RSID pair, the sink will
  // be notified of all RTCP packets with any of those sender-SSRCs.
  void AddMidRsidSink(const std::string& mid,
                      const std::string& rsid,
                      RtcpPacketSinkInterface* sink);

  // Registers a sink. Once an SSRC is bound to the payload type, the sink will
  // be notified of all RTCP packets with any of those sender-SSRCs.
  void AddPayloadTypeSink(uint8_t payload_type, RtcpPacketSinkInterface* sink);

  // Registers a sink. The sink will be notified of any incoming RTCP packet.
  void AddBroadcastSink(RtcpPacketSinkInterface* sink);

  // Undo previous AddSink() calls with the given sink.
  void RemoveSink(const RtcpPacketSinkInterface* sink);

  // Undo AddBroadcastSink().
  void RemoveBroadcastSink(const RtcpPacketSinkInterface* sink);

  // Process a new RTCP packet and forward it to the appropriate sinks.
  void OnRtcpPacket(rtc::ArrayView<const uint8_t> packet);

  // Implement SsrcBindingObserver - notified when an SSRC -> RSID mapping is
  // discovered.
  void OnSsrcBoundToRsid(const std::string& rsid, uint32_t ssrc) override;

  // Implement SsrcBindingObserver - notified when an SSRC -> MID mapping is
  // discovered.
  void OnSsrcBoundToMid(const std::string& mid, uint32_t ssrc) override;

  // Implement SsrcBindingObserver - notified when an SSRC -> MID, RSID mapping
  // is discovered.
  void OnSsrcBoundToMidRsid(const std::string& mid,
                            const std::string& rsid,
                            uint32_t ssrc) override;

  // Implement SsrcBindingObserver - notified when an SSRC -> payload type
  // mapping is discovered.
  void OnSsrcBoundToPayloadType(uint8_t payload_type, uint32_t ssrc) override;

  // TODO(eladalon): Add the ability to resolve RSIDs and inform observers,
  // like in the RtpDemuxer case, once the relevant standard is finalized.

 private:
  // Validates and adds a sink to the given type map for a given key.
  // Intended to be called as implementation for Add*Sink methods.
  template <class Key>
  void AddSinkToMap(std::multimap<Key, RtcpPacketSinkInterface*>* map,
                    const Key& key,
                    RtcpPacketSinkInterface* sink) {
    RTC_DCHECK(sink);
    RTC_DCHECK(!ContainerHasKey(broadcast_sinks_, sink));
    RTC_DCHECK(!MultimapAssociationExists(*map, key, sink));
    map->emplace(key, sink);
  }

  // Adds any sink associated with the key in the map as SSRC sinks for the
  // given SSRC.
  // Intended to be called as implementation for OnSsrcBoundTo* methods.
  template <class Key>
  void BindSinksToSsrc(const std::multimap<Key, RtcpPacketSinkInterface*>& map,
                       const Key& key,
                       uint32_t ssrc) {
    auto it_range = map.equal_range(key);
    for (auto it = it_range.first; it != it_range.second; ++it) {
      RtcpPacketSinkInterface* sink = it->second;
      // Do not duplicate existing associations.
      if (!MultimapAssociationExists(ssrc_sinks_, ssrc, sink)) {
        AddSink(ssrc, sink);
      }
    }
  }

  // Records the association of various types to sinks.
  // Note that each of these maps except for ssrc_sinks_ will be modified only
  // by calls to Add*Sink and RemoveSink.
  // SSRC sinks will store any sink added with AddSink(ssrc, sink) as well as
  // any sink associated with a type that becomes bound to an SSRC.
  std::multimap<uint32_t, RtcpPacketSinkInterface*> ssrc_sinks_;
  std::multimap<std::string, RtcpPacketSinkInterface*> rsid_sinks_;
  std::multimap<std::string, RtcpPacketSinkInterface*> mid_sinks_;
  std::multimap<std::pair<std::string, std::string>, RtcpPacketSinkInterface*>
      mid_rsid_sinks_;
  std::multimap<uint8_t, RtcpPacketSinkInterface*> payload_type_sinks_;

  // Sinks which will receive notifications of all incoming RTCP packets.
  // Additional/removal of sinks is expected to be significantly less frequent
  // than RTCP message reception; container chosen for iteration performance.
  std::vector<RtcpPacketSinkInterface*> broadcast_sinks_;
};

}  // namespace webrtc

#endif  // WEBRTC_CALL_RTCP_DEMUXER_H_
