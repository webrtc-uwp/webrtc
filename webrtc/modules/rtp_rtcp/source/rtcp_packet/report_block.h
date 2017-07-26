/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_REPORT_BLOCK_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_REPORT_BLOCK_H_

#include "webrtc/rtc_base/basictypes.h"
#include "webrtc/rtc_base/deprecation.h"

namespace webrtc {
namespace rtcp {

class ReportBlock {
 public:
  static const size_t kLength = 24;

  ReportBlock();
  ~ReportBlock() {}

  bool Parse(const uint8_t* buffer, size_t length);

  // Fills buffer with the ReportBlock.
  // Consumes ReportBlock::kLength bytes.
  void Create(uint8_t* buffer) const;

  void SetSourceSsrc(uint32_t ssrc) { source_ssrc_ = ssrc; }

  RTC_DEPRECATED void SetMediaSsrc(uint32_t ssrc) { SetSourceSsrc(ssrc); }
  void SetFractionLost(uint8_t fraction_lost) {
    fraction_lost_ = fraction_lost;
  }
  bool SetPacketsLost(uint32_t packets_lost);
  RTC_DEPRECATED bool SetCumulativeLost(uint32_t packets_lost) {
    return SetPacketsLost(packets_lost);
  }
  void SetExtendedHighestSequenceNumber(
      uint32_t extended_highest_sequence_number) {
    extended_highest_sequence_number_ = extended_highest_sequence_number;
  }
  RTC_DEPRECATED void SetExtHighestSeqNum(uint32_t ext_high_seq_num) {
    SetExtendedHighestSequenceNumber(ext_high_seq_num);
  }
  void SetJitter(uint32_t jitter) { jitter_ = jitter; }
  void SetLastSenderReportTimestamp(uint32_t last_sender_report_timestamp) {
    last_sender_report_timestamp_ = last_sender_report_timestamp;
  }
  RTC_DEPRECATED void SetLastSr(uint32_t last_sr) {
    SetLastSenderReportTimestamp(last_sr);
  }
  void SetDelaySinceLastSenderReport(uint32_t delay_since_last_sender_report) {
    delay_since_last_sender_report_ = delay_since_last_sender_report;
  }
  RTC_DEPRECATED void SetDelayLastSr(uint32_t delay_last_sr) {
    SetDelaySinceLastSenderReport(delay_last_sr);
  }

  uint32_t source_ssrc() const { return source_ssrc_; }
  uint8_t fraction_lost() const { return fraction_lost_; }
  uint32_t packets_lost() const { return packets_lost_; }
  RTC_DEPRECATED uint32_t cumulative_lost() const { return packets_lost_; }
  uint32_t extended_highest_sequence_number() const {
    return extended_highest_sequence_number_;
  }
  RTC_DEPRECATED uint32_t extended_high_seq_num() const {
    return extended_highest_sequence_number();
  }
  uint32_t jitter() const { return jitter_; }
  uint32_t last_sender_report_timestamp() const {
    return last_sender_report_timestamp_;
  }
  RTC_DEPRECATED uint32_t last_sr() const {
    return last_sender_report_timestamp();
  }
  uint32_t delay_since_last_sender_report() const {
    return delay_since_last_sender_report_;
  }
  RTC_DEPRECATED uint32_t delay_since_last_sr() const {
    return delay_since_last_sender_report();
  }

 private:
  uint32_t source_ssrc_;
  uint8_t fraction_lost_;
  uint32_t packets_lost_;
  uint32_t extended_highest_sequence_number_;
  uint32_t jitter_;
  uint32_t last_sender_report_timestamp_;
  uint32_t delay_since_last_sender_report_;
};

}  // namespace rtcp
}  // namespace webrtc
#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_REPORT_BLOCK_H_
