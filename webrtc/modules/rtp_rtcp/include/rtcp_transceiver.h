/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_INCLUDE_RTCP_TRANSCEIVER_H_
#define WEBRTC_MODULES_RTP_RTCP_INCLUDE_RTCP_TRANSCEIVER_H_

#include <memory>
#include <string>

#include "webrtc/rtc_base/constructormagic.h"
#include "webrtc/rtc_base/task_queue.h"
#include "webrtc/rtc_base/thread_checker.h"

namespace webrtc {
class ReceiveStatisticsProvider;
class Transport;
//
// Manage incoming and outgoing rtcp messages for multiple BUNDLED streams.
//
// This class is thread-safe.
class RtcpTransceiver {
 public:
  struct Configuration {
    Configuration();
    ~Configuration();
    // Logs the error and returns false if configuration miss key objects or
    // is inconsistant. May log warnings.
    bool Valid() const;

    // Used to prepend all log messages and to name task queue. Can be empty.
    std::string debug_id;

    // Ssrc to use for transport-wide feedbacks.
    uint32_t feedback_ssrc = 1;

    // cname of the local particiapnt.
    std::string cname;

    // Maximum packet size outgoing transport accepts.
    size_t max_packet_size = 1200;
    // Transport to send rtcp packets to. Should be set.
    Transport* outgoing_transport = nullptr;

    // Period to send receiver reports and attached messages.
    int min_periodic_report_ms = 1000;
    // class to use to generate report blocks in receiver reports.
    ReceiveStatisticsProvider* receive_statistics = nullptr;
  };
  explicit RtcpTransceiver(const Configuration& config);
  ~RtcpTransceiver();

  // Sends receiver report asap.
  void ForceSendReport();

 private:
  // Schedule periodic receiver report.
  void ScheduleReport();
  // Sends receiver report.
  void SendReport() RTC_RUN_ON(task_queue_);
  // Returns desired time between reports.
  int64_t ReportPeriodMs() const RTC_RUN_ON(task_queue_);

  const Configuration config_;
  rtc::TaskQueue task_queue_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(RtcpTransceiver);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_RTP_RTCP_INCLUDE_RTCP_TRANSCEIVER_H_
