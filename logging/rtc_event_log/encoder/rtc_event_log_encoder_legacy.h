/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_RTC_EVENT_LOG_ENCODER_RTC_EVENT_LOG_ENCODER_LEGACY_H_
#define LOGGING_RTC_EVENT_LOG_ENCODER_RTC_EVENT_LOG_ENCODER_LEGACY_H_

#include <memory>
#include <string>

#include "logging/rtc_event_log/encoder/rtc_event_log_encoder.h"
#include "rtc_base/buffer.h"

namespace webrtc {

namespace rtclog {
class Event;
}  // namespace rtclog

class RtcEventAudioNetworkAdaptation;
class RtcEventAudioPlayout;
class RtcEventAudioReceiveStreamConfig;
class RtcEventAudioSendStreamConfig;
class RtcEventBweUpdateDelayBased;
class RtcEventBweUpdateLossBased;
class RtcEventLoggingStarted;
class RtcEventLoggingStopped;
class RtcEventProbeClusterCreated;
class RtcEventProbeResultFailure;
class RtcEventProbeResultSuccess;
class RtcEventRtcpPacketIncoming;
class RtcEventRtcpPacketOutgoing;
class RtcEventRtpPacketIncoming;
class RtcEventRtpPacketOutgoing;
class RtcEventVideoReceiveStreamConfig;
class RtcEventVideoSendStreamConfig;
class RtpPacket;

class RtcEventLogEncoderLegacy final : public RtcEventLogEncoder {
 public:
  ~RtcEventLogEncoderLegacy() override = default;

  std::string Encode(const RtcEvent& event) override;

 private:
  // Encoding entry-point for the various RtcEvent subclasses.
  std::string AudioNetworkAdaptation(
      const RtcEventAudioNetworkAdaptation& event);
  std::string AudioPlayout(const RtcEventAudioPlayout& event);
  std::string AudioReceiveStreamConfig(
      const RtcEventAudioReceiveStreamConfig& event);
  std::string AudioSendStreamConfig(const RtcEventAudioSendStreamConfig& event);
  std::string BweUpdateDelayBased(const RtcEventBweUpdateDelayBased& event);
  std::string BweUpdateLossBased(const RtcEventBweUpdateLossBased& event);
  std::string LoggingStarted(const RtcEventLoggingStarted& event);
  std::string LoggingStopped(const RtcEventLoggingStopped& event);
  std::string ProbeClusterCreated(const RtcEventProbeClusterCreated& event);
  std::string ProbeResultFailure(const RtcEventProbeResultFailure& event);
  std::string ProbeResultSuccess(const RtcEventProbeResultSuccess&);
  std::string RtcpPacketIncoming(const RtcEventRtcpPacketIncoming& event);
  std::string RtcpPacketOutgoing(const RtcEventRtcpPacketOutgoing& event);
  std::string RtpPacketIncoming(const RtcEventRtpPacketIncoming& event);
  std::string RtpPacketOutgoing(const RtcEventRtpPacketOutgoing& event);
  std::string VideoReceiveStreamConfig(
      const RtcEventVideoReceiveStreamConfig& event);
  std::string VideoSendStreamConfig(const RtcEventVideoSendStreamConfig& event);

  // RTCP/RTP are handled similarly for incoming/outgoing.
  std::string RtcpPacket(int64_t timestamp_us,
                         const rtc::Buffer& packet,
                         bool is_incoming);
  std::string RtpPacket(int64_t timestamp_us,
                        const RtpPacket& header,
                        size_t packet_length,
                        bool is_incoming);

  std::string Serialize(rtclog::Event* event);
};

}  // namespace webrtc

#endif  // LOGGING_RTC_EVENT_LOG_ENCODER_RTC_EVENT_LOG_ENCODER_LEGACY_H_
