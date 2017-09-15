/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_INCLUDE_RTCP_TRANSCEIVER_H_
#define MODULES_RTP_RTCP_INCLUDE_RTCP_TRANSCEIVER_H_

#include <memory>
#include <string>

#include "modules/include/module.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/thread_annotations.h"
#include "rtc_base/thread_checker.h"

namespace webrtc {
class ReceiveStatisticsProvider;
class Transport;
//
// Manage incoming and outgoing rtcp messages for multiple BUNDLED streams.
//
// This class is thread-safe.
class RtcpTransceiver : public Module {
 public:
  struct Configuration {
    Configuration();
    ~Configuration();
    // Logs the error and returns false if configuration miss key objects or
    // is inconsistant. May log warnings.
    bool Valid() const;

    // Used to prepend all log messages. Can be empty.
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
  ~RtcpTransceiver() override;

  // Sends receiver report asap.
  void ForceSendReport();

 private:
  // Implements Module.
  void Process() override;
  int64_t TimeUntilNextProcess() override;
  void ProcessThreadAttached(ProcessThread* process_thread) override;

  // Sends receiver report.
  void SendReport();
  // Returns desired time between reports.
  int64_t ReportPeriodMs() const;

  const Configuration config_;

  rtc::ThreadChecker process_checker_;
  rtc::CriticalSection periodic_report_cs_;
  int64_t next_report_ms_ RTC_GUARDED_BY(process_checker_);
  ProcessThread* process_thread_ RTC_GUARDED_BY(periodic_report_cs_);

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(RtcpTransceiver);
};

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_INCLUDE_RTCP_TRANSCEIVER_H_
