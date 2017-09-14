/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/include/rtcp_transceiver.h"

#include "webrtc/api/call/transport.h"
#include "webrtc/modules/rtp_rtcp/include/receive_statistics.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/receiver_report.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/sdes.h"
#include "webrtc/rtc_base/logging.h"
#include "webrtc/rtc_base/ptr_util.h"
#include "webrtc/rtc_base/timeutils.h"

namespace webrtc {
namespace {

// Helper to generate compound rtcp packets.
class PacketSender : public rtcp::RtcpPacket::PacketReadyCallback {
 public:
  PacketSender(Transport* transport, size_t max_packet_size)
      : transport_(transport), max_packet_size_(max_packet_size) {}
  ~PacketSender() { RTC_DCHECK_EQ(index_, 0) << "Unsent rtcp packet."; }

  void AddBlock(const rtcp::RtcpPacket& block) {
    block.Create(buffer_, &index_, max_packet_size_, this);
  }

  void Send() {
    if (index_ > 0) {
      OnPacketReady(buffer_, index_);
      index_ = 0;
    }
  }

 private:
  // Implements RtcpPacket::PacketReadyCallback
  void OnPacketReady(uint8_t* data, size_t length) override {
    transport_->SendRtcp(data, length);
  }

  Transport* const transport_;
  const size_t max_packet_size_;
  size_t index_ = 0;
  uint8_t buffer_[IP_PACKET_SIZE];
};

}  // namespace

RtcpTransceiver::Configuration::Configuration() = default;
RtcpTransceiver::Configuration::~Configuration() = default;

bool RtcpTransceiver::Configuration::Valid() const {
  if (feedback_ssrc == 0)
    LOG(LS_WARNING)
        << debug_id
        << "Ssrc 0 may be treated by some implementation as invalid.";
  if (cname.size() > 255) {
    LOG(LS_ERROR) << debug_id << "cname can be maximum 255 characters.";
    return false;
  }
  if (max_packet_size < 100) {
    LOG(LS_ERROR) << debug_id << "max packet size " << max_packet_size
                  << " is too small.";
    return false;
  }
  if (max_packet_size > IP_PACKET_SIZE) {
    LOG(LS_ERROR) << debug_id << "max packet size " << max_packet_size
                  << " more than " << IP_PACKET_SIZE << " is unsupported.";
    return false;
  }
  if (outgoing_transport == nullptr) {
    LOG(LS_ERROR) << debug_id << "outgoing transport must be set";
    return false;
  }
  if (min_periodic_report_ms <= 0) {
    LOG(LS_ERROR) << debug_id << "period " << min_periodic_report_ms
                  << "ms between reports should be positive.";
    return false;
  }
  if (receive_statistics == nullptr)
    LOG(LS_WARNING)
        << debug_id
        << "receive statistic should be set to generate rtcp report blocks.";
  return true;
}

RtcpTransceiver::RtcpTransceiver(const RtcpTransceiver::Configuration& config)
    : config_(config), task_queue_((config.debug_id + "rtcp").c_str()) {
  RTC_CHECK(config_.Valid());
  ScheduleReport();
}

RtcpTransceiver::~RtcpTransceiver() = default;

void RtcpTransceiver::ForceSendReport() {
  task_queue_.PostTask([this] {
    RTC_DCHECK_RUN_ON(&task_queue_);
    SendReport();
  });
}

void RtcpTransceiver::ScheduleReport() {
  // Task to send periodic receiver reports.
  class PeriodicReport : public rtc::QueuedTask {
   public:
    explicit PeriodicReport(RtcpTransceiver* transceiver)
        : next_run_ms_(rtc::TimeMillis()), transceiver_(*transceiver) {}
    ~PeriodicReport() override = default;

    bool Run() override {
      RTC_DCHECK_RUN_ON(&transceiver_.task_queue_);
      transceiver_.SendReport();
      int64_t now_ms = rtc::TimeMillis();
      next_run_ms_ += transceiver_.ReportPeriodMs();
      int64_t until_next_run_ms = next_run_ms_ - now_ms;
      transceiver_.task_queue_.PostDelayedTask(
          std::unique_ptr<QueuedTask>(this), until_next_run_ms);
      return false;
    }

   private:
    int64_t next_run_ms_ RTC_ACCESS_ON(transceiver_.task_queue_);
    RtcpTransceiver& transceiver_;
  };

  task_queue_.PostTask(rtc::MakeUnique<PeriodicReport>(this));
}

void RtcpTransceiver::SendReport() {
  PacketSender sender(config_.outgoing_transport, config_.max_packet_size);

  rtcp::ReceiverReport rr;
  rr.SetSenderSsrc(config_.feedback_ssrc);
  if (config_.receive_statistics) {
    // TODO(danilchap): Support sending more than
    // |ReceiverReport::kMaxNumberOfReportBlocks| per compound rtcp packet.
    auto report_blocks = config_.receive_statistics->RtcpReportBlocks(
        rtcp::ReceiverReport::kMaxNumberOfReportBlocks);
    // TODO(danilchap): Fill in last_sr/delay_since_last_sr fields when
    // sender reports will be handled.
    rr.SetReportBlocks(std::move(report_blocks));
  }
  sender.AddBlock(rr);

  if (!config_.cname.empty()) {
    rtcp::Sdes sdes;
    bool added = sdes.AddCName(config_.feedback_ssrc, config_.cname);
    RTC_DCHECK(added) << "Failed to add cname " << config_.cname
                      << " to rtcp sdes packet.";
    sender.AddBlock(sdes);
  }
  sender.Send();
}

int64_t RtcpTransceiver::ReportPeriodMs() const {
  return config_.min_periodic_report_ms;
}

}  // namespace webrtc
