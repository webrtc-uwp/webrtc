/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_CALL_RTP_DEMUXER_H_
#define WEBRTC_CALL_RTP_DEMUXER_H_

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "webrtc/rtc_base/constructormagic.h"

namespace webrtc {

class RtpPacketReceived;
class RtpPacketSinkInterface;
class SsrcBindingObserver;

// This struct describes the criteria that will be used to match packets to a
// specific sink.
struct RtpDemuxerCriteria {
  RtpDemuxerCriteria();
  ~RtpDemuxerCriteria();

  // If not the empty string, will match packets with this MID.
  std::string mid;

  // If not the empty string, will match packets with this RTP stream ID.
  // Note that if both mid and rsid are specified, this will only match packets
  // that have both specified (either through RTP header extensions, SSRC
  // latching or RTCP).
  std::string rsid;

  // Will match packets with any of these SSRCs.
  std::vector<uint32_t> ssrcs;

  // Will match packets with any of these payload types.
  std::vector<uint8_t> payload_types;
};

// This class represents the RTP demuxing, for a single RTP session (i.e., one
// ssrc space, see RFC 7656). It isn't thread aware, leaving responsibility of
// multithreading issues to the user of this class.
// The demuxing algorithm follows the sketch given in the BUNDLE draft:
// https://tools.ietf.org/html/draft-ietf-mmusic-sdp-bundle-negotiation-38#section-10.2
// with modifications to support RTP stream IDs also.
//
// When a packet is received, the RtpDemuxer will route according to the
// following rules:
// 1. If the packet contains the MID header extension, and no sink has been
//    added with that MID as a criteria, the packet is not routed.
// 2. If the packet has a MID (either by the header extension or a previous SSRC
//    binding) and there is a sink added for that exact MID only, then route to
//    that sink and stop.
// 3. If the packet has a MID and an RSID (either by the header extension or a
//    previous SSRC binding) and there is a sink added for that MID, RSID pair,
//    then route to that sink and stop.
// 4. If the packet has an RSID and there is a sink added for that exact RSID
//    only, then route to that sink and stop.
// 5. If there is a sink added for the packet's SSRC, then route to that sink
//    and stop. Note that we expect the sink to check for an appropriate payload
//    type.
// 6. If there is exactly one sink added for the packet's payload type, then
//    route to that sink and stop.
// 7. Otherwise, the packet is not routed.
//
// In summary, the routing algorithm will always try to first match MID and RSID
// (including through SSRC binding), match SSRC directly as needed, and use
// payload types only if all else fails.
class RtpDemuxer {
 public:
  RtpDemuxer();
  ~RtpDemuxer();

  RtpDemuxer(const RtpDemuxer&) = delete;
  void operator=(const RtpDemuxer&) = delete;

  // Registers a sink that will be notified when RTP packets match its given
  // criteria according to the algorithm described in the class description.
  // Returns true if the sink was successfully added.
  // Returns false in the following situations:
  // - Only MID is specified and the MID is already registered.
  // - Only RSID is specified and the RSID is already registered.
  // - Both MID and RSID is specified and the (MID, RSID) pair is already
  //   registered.
  // - Any of the criteria SSRCs are already registered.
  // If false is returned, no changes are made to the demuxer state.
  bool AddSink(const RtpDemuxerCriteria& criteria,
               RtpPacketSinkInterface* sink);

  // Registers a sink. Multiple SSRCs may be mapped to the same sink, but
  // each SSRC may only be mapped to one sink. The return value reports
  // whether the association has been recorded or rejected. Rejection may occur
  // if the SSRC has already been associated with a sink. The previously added
  // sink is *not* forgotten.
  bool AddSink(uint32_t ssrc, RtpPacketSinkInterface* sink);

  // Registers a sink's association to an RSID. Only one sink may be associated
  // with a given RSID. Null pointer is not allowed.
  void AddSink(const std::string& rsid, RtpPacketSinkInterface* sink);

  // Removes a sink. Return value reports if anything was actually removed.
  // Null pointer is not allowed.
  bool RemoveSink(const RtpPacketSinkInterface* sink);

  // Handles RTP packets. Returns true if at least one matching sink was found.
  // Performs RTP demuxing on the given packet. Returns true if a matching sink
  // was notified of the packet.
  bool OnRtpPacket(const RtpPacketReceived& packet);

  // The Observer will be notified when an attribute (e.g., RSID, MID, etc.) is
  // bound to an SSRC.
  void RegisterSsrcBindingObserver(SsrcBindingObserver* observer);

  // Undo a previous RegisterSsrcBindingObserver().
  void DeregisterSsrcBindingObserver(const SsrcBindingObserver* observer);

 private:
  // Returns true if adding a sink with the given criteria would cause conflicts
  // with the existing criteria and should be rejected.
  bool CriteriaWouldConflict(const RtpDemuxerCriteria& criteria) const;

  // Runs the demux algorithm on the given packet and returns the sink that
  // should receive the packet.
  // Will record any SSRC<->ID associations along the way.
  // If the packet should be dropped, this method returns null.
  RtpPacketSinkInterface* ResolveSink(const RtpPacketReceived& packet);

  // Returns a pointer to the MID, if found, from the following sources:
  // 1. packet_mid, if not empty
  // 2. Stored mid_by_ssrc_ value for the given SSRC, if found.
  // Otherwise, returns nullptr.
  const std::string* ResolveMid(const std::string& packet_mid,
                                uint32_t ssrc) const;

  // Returns a pointer to the RSID, if found, from the following sources:
  // 1. packet_rsid, if not empty
  // 2. packet_rrid, if not empty
  // 3. Stored rsid_by_ssrc_ value for the given SSRC, if found.
  // Otherwise, returns nullptr.
  const std::string* ResolveRsid(const std::string& packet_rsid,
                                 const std::string& packet_rrid,
                                 uint32_t ssrc) const;

  // Used by the ResolveSink algorithm.
  RtpPacketSinkInterface* ResolveSinkByMid(const std::string& mid,
                                           uint32_t ssrc);
  RtpPacketSinkInterface* ResolveSinkByMidRsid(const std::string& mid,
                                               const std::string& rsid,
                                               uint32_t ssrc);
  RtpPacketSinkInterface* ResolveSinkByRsid(const std::string& rsid,
                                            uint32_t ssrc);
  RtpPacketSinkInterface* ResolveSinkByPayloadType(uint8_t payload_type,
                                                   uint32_t ssrc);

  // Map each sink by its component attributes to facilitate quick lookups.
  // Payload Type mapping is a multimap because if two sinks register for the
  // same payload type, both AddSinks succeed but we must know not to demux on
  // that attribute since it is ambiguous.
  // Note: Mappings are only modified by AddSink/RemoveSink (except for
  // SSRC mapping which receives all MID, payload type, or RSID to SSRC bindings
  // discovered when demuxing packets).
  std::map<std::string, RtpPacketSinkInterface*> sink_by_mid_;
  std::map<uint32_t, RtpPacketSinkInterface*> sink_by_ssrc_;
  std::multimap<uint8_t, RtpPacketSinkInterface*> sinks_by_pt_;
  std::map<std::pair<std::string, std::string>, RtpPacketSinkInterface*>
      sink_by_mid_and_rsid_;
  std::map<std::string, RtpPacketSinkInterface*> sink_by_rsid_;

  // Tracks all the MIDs that have been identified in added criteria. Used to
  // determine if a packet should be dropped right away because the MID is
  // unknown.
  std::set<std::string> known_mids_;

  // Regenerate the known_mids_ set from information in the sink_by_mid and
  // sink_by_mid_and_rsid_ maps.
  void RefreshKnownMids();

  // Records learned mappings of mid --> SSRC and rsid --> SSRC as packets are
  // received.
  // This is stored separately from the sink mappings because if a sink is
  // removed we want to still remember these associations.
  std::map<uint32_t, std::string> mid_by_ssrc_;
  std::map<uint32_t, std::string> rsid_by_ssrc_;

  // Adds a binding from the SSRC to the given sink. Returns true if there was
  // not already a sink bound to the SSRC or if the sink replaced a different
  // sink. Returns false if the binding was unchanged.
  bool AddSsrcSinkBinding(uint32_t ssrc, RtpPacketSinkInterface* sink);

  // Observers which will be notified when an RSID association to an SSRC is
  // resolved by this object.
  std::vector<SsrcBindingObserver*> ssrc_binding_observers_;

  // Calls the given function for every registered observer.
  template <class Fn>
  void NotifyObservers(Fn&& fn) {
    for (auto* observer : ssrc_binding_observers_) {
      fn(observer);
    }
  }
};

}  // namespace webrtc

#endif  // WEBRTC_CALL_RTP_DEMUXER_H_
