// Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "webrtc/build/WinUWP_gyp/Stats/webrtc_stats_network_sender.h"
#include "webrtc/build/WinUWP_gyp/Stats/etw_providers.h"

#include "webrtc/api/peerconnectioninterface.h"
#include "webrtc/base/asynctcpsocket.h" 
#include "webrtc/base/json.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/physicalsocketserver.h"

namespace webrtc {
 
WebRTCStatsNetworkSender::WebRTCStatsNetworkSender() : 
  thread_(nullptr),
  message_start_marker_(0x02 /*STX*/),
  message_end_marker_(0x03 /*ETX*/) {
}

WebRTCStatsNetworkSender::~WebRTCStatsNetworkSender() {
  if (IsRunning()) {
    Stop();
  }
}

bool WebRTCStatsNetworkSender::Start(std::string remote_hostname, int remote_port) {
  if (IsRunning()) {
    LOG(LS_INFO) << "WebRTCStatsNetworkSender already started";
    return false;
  }

  char computer_name[256];
  if (::gethostname(computer_name, sizeof(computer_name)) == 0) {
    local_host_name_ = computer_name;
  }
  else {
    local_host_name_ = "N/A";
  }

  rtc::PhysicalSocketServer* pss = new rtc::PhysicalSocketServer();
  thread_ = new rtc::Thread(pss);
  thread_->SetName("WebRTCStatsNetworkSender", nullptr);

  rtc::SocketAddress localAddress(INADDR_ANY, 0);
  rtc::SocketAddress remoteAddress(remote_hostname, remote_port);

  socket_.reset(
    thread_->socketserver()->CreateAsyncSocket(AF_INET, SOCK_STREAM));

  if (socket_ == nullptr) {
    LOG(LS_ERROR) << "WebRTCStatsNetworkSender failed to start";
    return false;
  }
  socket_->Connect(remoteAddress);
  thread_->Start();
  return true;
}

bool WebRTCStatsNetworkSender::Stop() {
  if (!IsRunning()) {
    LOG(LS_INFO) << "WebRTCStatsNetworkSender not running";
    return false;
  }

  if (thread_ != nullptr) {
    thread_->Stop();
  }

  socket_->Close();
  socket_.reset();
  return true;
}

bool WebRTCStatsNetworkSender::IsRunning() {
  return socket_ != nullptr;
}

bool WebRTCStatsNetworkSender::ProcessStats(const StatsReports& reports,
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> pci) {
  if (socket_ == nullptr || socket_->GetState() != rtc::Socket::CS_CONNECTED) {
    return false;
  }
  if (pci == nullptr) {
    return false;
  }

  Json::Value jmessage;
  jmessage["hostname"] = local_host_name_;

  std::vector<Json::Value> jreports;
  size_t stats_count = 0;

  for (auto report : reports) {
    std::string sgn = report->id()->ToString();
    auto stat_group_name = sgn.c_str();
    auto stat_type = report->id()->type();
    auto timestamp = report->timestamp();

    bool send = false;
    if (stat_type == StatsReport::kStatsReportTypeSession ||
      stat_type == StatsReport::kStatsReportTypeTrack ||
      stat_type == StatsReport::kStatsReportTypeBwe) {
      send = true;
    }
    else if (stat_type == StatsReport::kStatsReportTypeSsrc) {
      const StatsReport::Value* v = report->FindValue(
        StatsReport::kStatsValueNameTrackId);
      if (v) {
        const std::string& id = v->string_val();
        auto ls = pci->local_streams();
        auto rs = pci->remote_streams();
        if (ls->FindAudioTrack(id) || ls->FindVideoTrack(id) ||
          rs->FindAudioTrack(id) || rs->FindVideoTrack(id)) {
          send = true;
        }
      }
    }
    if (!send) {
      continue;
    }
    Json::Value jreport;
    jreport["gr_n"] = stat_group_name;
    jreport["ts"] = timestamp;
    std::vector<Json::Value> jstats;

    for (auto value : report->values()) {
      auto stat_name = value.second->display_name();
      Json::Value jstat;
      jstat["n"] = stat_name;

      switch (value.second->type()) {
      case StatsReport::Value::kInt:
        jstat["t"] = StatsReportInt32.Id;
        jstat["v"] = value.second->int_val();
        break;
      case StatsReport::Value::kInt64:
        jstat["t"] = StatsReportInt64.Id;
        jstat["v"] = value.second->int64_val();
        break;
      case StatsReport::Value::kFloat:
        jstat["t"] = StatsReportFloat.Id;
        jstat["v"] = value.second->float_val();
        break;
      case StatsReport::Value::kBool:
        jstat["t"] = StatsReportBool.Id;
        jstat["v"] = value.second->bool_val();
        break;
      case StatsReport::Value::kStaticString:
        jstat["t"] = StatsReportString.Id;
        jstat["v"] = value.second->static_string_val();
        break;
      case StatsReport::Value::kString:
        jstat["t"] = StatsReportString.Id;
        jstat["v"] = value.second->string_val().c_str();
        break;
      default:
        continue;
        break;
      }
      jstats.push_back(jstat);
    }
    if (!jstats.empty()) {
      jreport["stats"] = rtc::ValueVectorToJsonArray(jstats);
      jreports.push_back(jreport);
      stats_count += jstats.size();
    }
  }
  jmessage["groups"] = rtc::ValueVectorToJsonArray(jreports);
  jmessage["stat_cnt"] = stats_count;
  /*Json::StyledWriter writer;*/
  Json::FastWriter writer;
  auto data = writer.write(jmessage);
  socket_->Send(&message_start_marker_, 1);
  socket_->Send(data.c_str(), data.length());
  socket_->Send(&message_end_marker_, 1);
  return true;
}

}  // namespace webrtc