
// Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef WEBRTC_BUILD_WINUWP_GYP_STATS_WEBRTC_STATS_OBSERVER_H_
#define WEBRTC_BUILD_WINUWP_GYP_STATS_WEBRTC_STATS_OBSERVER_H_

#include <string>

#include "webrtc/api/peerconnectioninterface.h"
#include "webrtc/build/WinUWP_gyp/Api/RTCStatsReport.h"


namespace webrtc {
class CriticalSectionWrapper;
class WebRTCStatsObserverWinUWP;
class WebRTCStatsNetworkSender;

struct ConnectionHealthStats {
  ConnectionHealthStats();
  void Reset();
  double timestamp;
  int64 received_bytes;
  int64 received_kbps;
  int64 sent_bytes;
  int64 sent_kbps;
  int64 rtt;
  std::string local_candidate_type;
  std::string remote_candidate_type;
};

// A webrtc::StatsObserver implementation used to receive statistics about the
// current PeerConnection. The statistics are logged to an ETW session and/or
// sent to a WebRTCStatsObserverWinUWP.
class WebRTCStatsObserver : public StatsObserver, public rtc::MessageHandler {
 public:
  enum Status {
    kStopped = 0,
    kStarted = 1,
    kStopping = 2
  };
  WebRTCStatsObserver(rtc::scoped_refptr<webrtc::PeerConnectionInterface> pci);

  void ToggleETWStats(bool enable);
  void ToggleStatsSendToRemoteHost(bool enable);
  void ToggleConnectionHealthStats(WebRTCStatsObserverWinUWP* observer);
  void ToggleRTCStats(WebRTCStatsObserverWinUWP* observer);
  
  void SetStatsNetworkDestination(std::string remote_hostname, int remote_port);

  // StatsObserver
  virtual void OnComplete(const StatsReports& reports);

  // MessageHandler
  void OnMessage(rtc::Message* msg);

 protected:
  virtual ~WebRTCStatsObserver();

 private:
  void Start();
  void Stop();
  void GetStreamCollectionStats(
    rtc::scoped_refptr<StreamCollectionInterface>streams);
  void GetAllStats();
  void PollStats();

  void EvaluatePollNecessity();

 private:
  static const int kInterval;
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> pci_;

  rtc::scoped_ptr<CriticalSectionWrapper> crit_sect_;
  Status status_;
  WebRTCStatsObserverWinUWP* webrtc_stats_observer_winuwp_;
  bool etw_stats_enabled_;
  ConnectionHealthStats conn_health_stats_prev;
  ConnectionHealthStats conn_health_stats_;
  bool rtc_stats_enabled_;
  bool conn_health_stats_enabled_;

  bool rtc_stats_to_remote_host_enabled_;
  std::string stats_network_destination_hostname_;
  int stats_network_destination_port_;
  rtc::scoped_ptr<WebRTCStatsNetworkSender> network_sender_;
};

class WebRTCStatsObserverWinUWP {
 public:
  virtual void OnConnectionHealthStats(const ConnectionHealthStats& stats) = 0;

  virtual void OnRTCStatsReportsReady(
    const Org::WebRtc::RTCStatsReports& rtcStatsReports) = 0;
};

}  // namespace webrtc

#endif  //  WEBRTC_BUILD_WINUWP_GYP_STATS_WEBRTC_STATS_OBSERVER_H_
