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
#include <string>
#include <utility>
#include <vector>

#include "webrtc/rtc_base/constructormagic.h"

namespace webrtc {

class MidResolutionObserver;
class RsidResolutionObserver;
class RtpPacketReceived;
class RtpPacketSinkInterface;

// This struct describes the criteria that will be used to match packets to a
// specific sink.
struct RtpDemuxerCriteria {
  RtpDemuxerCriteria();
  ~RtpDemuxerCriteria();

  // If not the empty string, will match packets with this MID.
  std::string mid;

  // Will match packets with any of these SSRCs.
  std::vector<uint32_t> ssrcs;

  // Will match packets with any of these payload types.
  std::vector<uint8_t> payload_types;

  // Will match packets with any of these RTP stream IDs. If MID is also
  // specified, will match RSIDs scoped within the MID.
  std::vector<std::string> rsids;
};

// This class represents the RTP demuxing, for a single RTP session (i.e., one
// ssrc space, see RFC 7656). It isn't thread aware, leaving responsibility of
// multithreading issues to the user of this class.
// The demuxing algorithm follows the sketch given in the BUNDLE draft:
// https://tools.ietf.org/html/draft-ietf-mmusic-sdp-bundle-negotiation-38#section-10.2
// with additional support for RTP stream IDs.
class RtpDemuxer {
 public:
  RtpDemuxer();
  ~RtpDemuxer();

  RtpDemuxer(const RtpDemuxer&) = delete;
  void operator=(const RtpDemuxer&) = delete;

  // Registers a sink that will be notified when RTP packets uniquely match the
  // given criteria.
  // Returns true if the sink was successfully added.
  // Returns false in the following situations:
  // - Any of the criteria SSRCs are already registered.
  // - Any of the MID, RSID pairs are already registered.
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
  bool OnRtpPacket(const RtpPacketReceived& packet);

  // Allows other objects to be notified when RSID-SSRC associations are
  // resolved by this object.
  void RegisterRsidResolutionObserver(RsidResolutionObserver* observer);

  // Undo a previous RegisterRsidResolutionObserver().
  void DeregisterRsidResolutionObserver(const RsidResolutionObserver* observer);

 private:
  // Returns true if adding a sink with the given criteria would cause conflicts
  // with the existing criteria and should be prevented.
  bool CriteriaWouldConflict(const RtpDemuxerCriteria& criteria) const;

  // Returns the sink that is bound to the given SSRC, or null if not found.
  RtpPacketSinkInterface* GetSinkBySsrc(uint32_t ssrc) const;

  // Runs the demux algorithm on the given packet and returns the sink that
  // should receive the packet.
  // If the packet should be dropped, this method returns null.
  RtpPacketSinkInterface* FindSink(const RtpPacketReceived& packet);

  // If the second element of the returned pair is true, processing should stop
  // and the packet should be dropped.
  std::pair<RtpPacketSinkInterface*, bool> FindSinkByMid(
      const RtpPacketReceived& packet);
  RtpPacketSinkInterface* FindSinkByPayloadType(
      const RtpPacketReceived& packet);
  RtpPacketSinkInterface* FindSinkByRsid(const RtpPacketReceived& packet);

  // Notify observers of the resolution of an RSID to an SSRC.
  void NotifyObserversOfRsidResolution(const std::string& rsid, uint32_t ssrc);

  // Mappings for each attribute from values to the sinks that have that value
  // as their criteria.
  // MID and Payload Type mappings are multimap because it's possible for
  // different criteria to share attributes. If multiple sinks are associated
  // to a value, the demuxing algorithm will not use that attribute alone to
  // choose the receiving sink.
  // Note: Mappings are only modified by AddSink/RemoveSink (except for
  // SSRC mapping which receives all MID, payload type, or RSID to SSRC bindings
  // discovered when demuxing packets).
  std::multimap<std::string, RtpPacketSinkInterface*> sinks_by_mid_;
  // Note: SSRC bindings are unique.
  std::map<uint32_t, RtpPacketSinkInterface*> sinks_by_ssrc_;
  std::multimap<uint8_t, RtpPacketSinkInterface*> sinks_by_payload_type_;
  // Maps (MID, SSRC) --> Sink.
  // RSIDs are scoped within MIDs, but using RSID does require using MID.
  // MID value of empty string indicates RSID alone, no MID.
  std::map<std::pair<std::string, std::string>, RtpPacketSinkInterface*>
      sinks_by_mid_rsid_;

  // Observers which will be notified when an RSID association to an SSRC is
  // resolved by this object.
  std::vector<RsidResolutionObserver*> rsid_resolution_observers_;
};

}  // namespace webrtc

#endif  // WEBRTC_CALL_RTP_DEMUXER_H_
