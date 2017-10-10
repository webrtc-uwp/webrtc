// Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef WEBRTC_BUILD_WINUWP_GYP_STATS_WEBRTC_STATS_NETWORK_SENDER_H_
#define WEBRTC_BUILD_WINUWP_GYP_STATS_WEBRTC_STATS_NETWORK_SENDER_H_

#include <string>
#include "webrtc/api/statstypes.h"
#include "webrtc/base/sigslot.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/scoped_ref_ptr.h"

namespace rtc {
  class AsyncSocket;
  class Thread;
}
namespace webrtc {
class PeerConnectionInterface;

class WebRTCStatsNetworkSender : public sigslot::has_slots<sigslot::multi_threaded_local> {
public:
  WebRTCStatsNetworkSender();
  virtual ~WebRTCStatsNetworkSender();

  bool Start(std::string remote_hostname, int remote_port);
  bool Stop();
  bool IsRunning();
  bool ProcessStats(const StatsReports& reports, rtc::scoped_refptr<webrtc::PeerConnectionInterface> pci);

private:
  rtc::scoped_ptr<rtc::AsyncSocket> socket_;
  rtc::Thread* thread_;

  std::string local_host_name_;

  const char message_start_marker_;
  const char message_end_marker_;
};

}  // namespace webrtc

#endif  //  WEBRTC_BUILD_WINUWP_GYP_STATS_WEBRTC_STATS_NETWORK_SENDER_H_