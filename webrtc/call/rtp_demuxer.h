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
#include <vector>

#include "webrtc/rtc_base/constructormagic.h"
#include "webrtc/rtc_base/optional.h"

namespace webrtc {

class MidResolutionObserver;
class RsidResolutionObserver;
class RtpPacketReceived;
class RtpPacketSinkInterface;

struct RtpDemuxerCriteria {
  RtpDemuxerCriteria();
  RtpDemuxerCriteria(const RtpDemuxerCriteria& o);
  ~RtpDemuxerCriteria();

  std::vector<uint32_t> ssrcs;
  std::vector<uint8_t> payload_types;
  rtc::Optional<std::string> mid;
  rtc::Optional<std::string> rsid;
};

// This class represents the RTP demuxing, for a single RTP session (i.e., one
// ssrc space, see RFC 7656). It isn't thread aware, leaving responsibility of
// multithreading issues to the user of this class.
class RtpDemuxer {
 public:
  RtpDemuxer();
  ~RtpDemuxer();

  // Registers a sink, accepting RTP packets according to the given criteria.
  // Packets will be routed to this sink if they match more of this sink's
  // criteria than any other sink. For example, if packet P has payload type T
  // and two sinks both have T as a criteria, the packet will not be routed to
  // either unless there is another piece of information (e.g., an MID) that
  // matches the packet to one sink's criteria.
  // Fields are checked in the following order: SSRC, payload type, MID, RSID,
  // MID and RSID.
  void AddSink(RtpDemuxerCriteria criteria, RtpPacketSinkInterface* sink);

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
  bool TryDemuxWithSsrc(const RtpPacketReceived& packet);
  bool TryDemuxWithPayloadType(const RtpPacketReceived& packet);
  bool TryDemuxWithMid(const RtpPacketReceived& packet);
  bool TryDemuxWithRsid(const RtpPacketReceived& packet);
  bool TryDemuxWithMidRsid(const RtpPacketReceived& packet);

  // Returns the one sink that matches the given predicate function
  // (RtpDemuxerCriteria -> bool). If multiple sinks match, then nullptr is
  // returned.
  template <class UnaryPredicate>
  RtpPacketSinkInterface* FindSinkByPredicate(UnaryPredicate pred) const {
    RtpPacketSinkInterface* found_sink = nullptr;
    for (const auto& item : sinks_) {
      const RtpDemuxerCriteria& criteria = item.first;
      RtpPacketSinkInterface* sink = item.second;
      if (pred(criteria)) {
        if (found_sink != nullptr) {
          return nullptr;
        }
        found_sink = sink;
      }
    }
    return found_sink;
  }

  std::vector<std::pair<RtpDemuxerCriteria, RtpPacketSinkInterface*> > sinks_;

  // Binds the given sink to the given SSRC if a binding does not already exist.
  // Returns true if the binding was created.
  bool AddSsrcSinkBinding(uint32_t ssrc, RtpPacketSinkInterface* sink);

  // Notify observers of the resolution of an RSID to an SSRC.
  void NotifyObserversOfRsidResolution(const std::string& rsid, uint32_t ssrc);

  // This records the association SSRCs to sinks. Other associations, such
  // as by RSID, also end up here once the RSID, etc., is resolved to an SSRC.
  std::map<uint32_t, RtpPacketSinkInterface*> ssrc_sink_mapping_;

  // Observers which will be notified when an RSID association to an SSRC is
  // resolved by this object.
  std::vector<RsidResolutionObserver*> rsid_resolution_observers_;
};

}  // namespace webrtc

#endif  // WEBRTC_CALL_RTP_DEMUXER_H_
