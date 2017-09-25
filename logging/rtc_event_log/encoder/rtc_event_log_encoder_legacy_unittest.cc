/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string>

#include "logging/rtc_event_log/encoder/rtc_event_log_encoder_legacy.h"
#include "logging/rtc_event_log/events/rtc_event_audio_network_adaptation.h"
#include "logging/rtc_event_log/events/rtc_event_audio_playout.h"
#include "logging/rtc_event_log/events/rtc_event_audio_receive_stream_config.h"
#include "logging/rtc_event_log/events/rtc_event_audio_send_stream_config.h"
#include "logging/rtc_event_log/events/rtc_event_bwe_update_delay_based.h"
#include "logging/rtc_event_log/events/rtc_event_bwe_update_loss_based.h"
#include "logging/rtc_event_log/events/rtc_event_logging_started.h"
#include "logging/rtc_event_log/events/rtc_event_logging_stopped.h"
#include "logging/rtc_event_log/events/rtc_event_probe_cluster_created.h"
#include "logging/rtc_event_log/events/rtc_event_probe_result_failure.h"
#include "logging/rtc_event_log/events/rtc_event_probe_result_success.h"
#include "logging/rtc_event_log/events/rtc_event_rtcp_packet_incoming.h"
#include "logging/rtc_event_log/events/rtc_event_rtcp_packet_outgoing.h"
#include "logging/rtc_event_log/events/rtc_event_rtp_packet_incoming.h"
#include "logging/rtc_event_log/events/rtc_event_rtp_packet_outgoing.h"
#include "logging/rtc_event_log/events/rtc_event_video_receive_stream_config.h"
#include "logging/rtc_event_log/events/rtc_event_video_send_stream_config.h"
#include "logging/rtc_event_log/rtc_event_log_parser.h"
#include "modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/safe_conversions.h"
#include "test/gtest.h"

namespace webrtc {

class RtcEventLogEncoderLegacyTest : public testing::Test {
 public:
  ~RtcEventLogEncoderLegacyTest() override = default;

 protected:
  RtcEventLogEncoderLegacy encoder;
  ParsedRtcEventLog parsed_log;
  // TODO(eladalon): Consider using a random input for the value, to demonstrate
  // that encoding works correctly in general, and not only over those values
  // which I happened to choose.
};

TEST_F(RtcEventLogEncoderLegacyTest, RtcEventAudioNetworkAdaptation) {}

TEST_F(RtcEventLogEncoderLegacyTest, RtcEventAudioPlayout) {
  constexpr uint32_t ssrc = 394;
  auto event = rtc::MakeUnique<RtcEventAudioPlayout>(ssrc);
  const int64_t timestamp_us = event->timestamp_us_;

  ASSERT_TRUE(parsed_log.ParseString(encoder.Encode(*event)));
  ASSERT_EQ(parsed_log.GetNumberOfEvents(), 1u);
  ASSERT_EQ(parsed_log.GetEventType(0), ParsedRtcEventLog::AUDIO_PLAYOUT_EVENT);

  uint32_t parsed_ssrc;
  parsed_log.GetAudioPlayout(0, &parsed_ssrc);

  EXPECT_EQ(parsed_log.GetTimestamp(0), timestamp_us);
  EXPECT_EQ(parsed_ssrc, ssrc);
}

TEST_F(RtcEventLogEncoderLegacyTest, RtcEventAudioReceiveStreamConfig) {}

TEST_F(RtcEventLogEncoderLegacyTest, RtcEventAudioSendStreamConfig) {}

TEST_F(RtcEventLogEncoderLegacyTest, RtcEventBweUpdateDelayBased) {
  constexpr int32_t bitrate_bps = 48992;
  constexpr BandwidthUsage detector_state = BandwidthUsage::kBwOverusing;
  auto event =
      rtc::MakeUnique<RtcEventBweUpdateDelayBased>(bitrate_bps, detector_state);
  const int64_t timestamp_us = event->timestamp_us_;

  ASSERT_TRUE(parsed_log.ParseString(encoder.Encode(*event)));
  ASSERT_EQ(parsed_log.GetNumberOfEvents(), 1u);
  ASSERT_EQ(parsed_log.GetEventType(0),
            ParsedRtcEventLog::DELAY_BASED_BWE_UPDATE);

  auto parsed_event = parsed_log.GetDelayBasedBweUpdate(0);

  EXPECT_EQ(parsed_log.GetTimestamp(0), timestamp_us);
  EXPECT_EQ(parsed_event.bitrate_bps, bitrate_bps);
  EXPECT_EQ(parsed_event.detector_state, detector_state);
}

TEST_F(RtcEventLogEncoderLegacyTest, RtcEventBweUpdateLossBased) {
  constexpr int32_t bitrate_bps = 8273;
  constexpr uint8_t fraction_loss = 12;
  constexpr int32_t total_packets = 99;

  auto event = rtc::MakeUnique<RtcEventBweUpdateLossBased>(
      bitrate_bps, fraction_loss, total_packets);
  const int64_t timestamp_us = event->timestamp_us_;

  ASSERT_TRUE(parsed_log.ParseString(encoder.Encode(*event)));
  ASSERT_EQ(parsed_log.GetNumberOfEvents(), 1u);
  ASSERT_EQ(parsed_log.GetEventType(0),
            ParsedRtcEventLog::LOSS_BASED_BWE_UPDATE);

  int32_t parsed_bitrate_bps;
  uint8_t parsed_fraction_loss;
  int32_t parsed_total_packets;
  parsed_log.GetLossBasedBweUpdate(
      0, &parsed_bitrate_bps, &parsed_fraction_loss, &parsed_total_packets);

  EXPECT_EQ(parsed_log.GetTimestamp(0), timestamp_us);
  EXPECT_EQ(parsed_bitrate_bps, bitrate_bps);
  EXPECT_EQ(parsed_fraction_loss, fraction_loss);
  EXPECT_EQ(parsed_total_packets, total_packets);
}

TEST_F(RtcEventLogEncoderLegacyTest, RtcEventLoggingStarted) {
  auto event = rtc::MakeUnique<RtcEventLoggingStarted>();
  const int64_t timestamp_us = event->timestamp_us_;

  ASSERT_TRUE(parsed_log.ParseString(encoder.Encode(*event)));
  ASSERT_EQ(parsed_log.GetNumberOfEvents(), 1u);
  ASSERT_EQ(parsed_log.GetEventType(0), ParsedRtcEventLog::LOG_START);

  EXPECT_EQ(parsed_log.GetTimestamp(0), timestamp_us);
}

TEST_F(RtcEventLogEncoderLegacyTest, RtcEventLoggingStopped) {
  auto event = rtc::MakeUnique<RtcEventLoggingStopped>();
  const int64_t timestamp_us = event->timestamp_us_;

  ASSERT_TRUE(parsed_log.ParseString(encoder.Encode(*event)));
  ASSERT_EQ(parsed_log.GetNumberOfEvents(), 1u);
  ASSERT_EQ(parsed_log.GetEventType(0), ParsedRtcEventLog::LOG_END);

  EXPECT_EQ(parsed_log.GetTimestamp(0), timestamp_us);
}

TEST_F(RtcEventLogEncoderLegacyTest, RtcEventProbeClusterCreated) {
  constexpr int id = 22;
  constexpr int bitrate_bps = 33;
  constexpr int min_probes = 44;
  constexpr int min_bytes = 55;

  auto event = rtc::MakeUnique<RtcEventProbeClusterCreated>(
      id, bitrate_bps, min_probes, min_bytes);
  const int64_t timestamp_us = event->timestamp_us_;

  ASSERT_TRUE(parsed_log.ParseString(encoder.Encode(*event)));
  ASSERT_EQ(parsed_log.GetNumberOfEvents(), 1u);
  ASSERT_EQ(parsed_log.GetEventType(0),
            ParsedRtcEventLog::BWE_PROBE_CLUSTER_CREATED_EVENT);

  auto parsed_event = parsed_log.GetBweProbeClusterCreated(0);

  EXPECT_EQ(parsed_log.GetTimestamp(0), timestamp_us);
  EXPECT_EQ(rtc::dchecked_cast<int>(parsed_event.id), id);
  EXPECT_EQ(rtc::dchecked_cast<int>(parsed_event.bitrate_bps), bitrate_bps);
  EXPECT_EQ(rtc::dchecked_cast<int>(parsed_event.min_packets), min_probes);
  EXPECT_EQ(rtc::dchecked_cast<int>(parsed_event.min_bytes), min_bytes);
}

TEST_F(RtcEventLogEncoderLegacyTest, RtcEventProbeResultFailure) {
  constexpr int id = 24;
  constexpr ProbeFailureReason failure_reason = kInvalidSendReceiveRatio;

  auto event = rtc::MakeUnique<RtcEventProbeResultFailure>(id, failure_reason);
  const int64_t timestamp_us = event->timestamp_us_;

  ASSERT_TRUE(parsed_log.ParseString(encoder.Encode(*event)));
  ASSERT_EQ(parsed_log.GetNumberOfEvents(), 1u);
  ASSERT_EQ(parsed_log.GetEventType(0),
            ParsedRtcEventLog::BWE_PROBE_RESULT_EVENT);

  auto parsed_event = parsed_log.GetBweProbeResult(0);

  EXPECT_EQ(parsed_log.GetTimestamp(0), timestamp_us);
  EXPECT_EQ(rtc::dchecked_cast<int>(parsed_event.id), id);
  ASSERT_FALSE(parsed_event.bitrate_bps);
  ASSERT_TRUE(parsed_event.failure_reason);
  EXPECT_EQ(parsed_event.failure_reason, failure_reason);
}

TEST_F(RtcEventLogEncoderLegacyTest, RtcEventProbeResultSuccess) {
  constexpr int id = 25;
  constexpr int bitrate_bps = 26;

  auto event = rtc::MakeUnique<RtcEventProbeResultSuccess>(id, bitrate_bps);
  const int64_t timestamp_us = event->timestamp_us_;

  ASSERT_TRUE(parsed_log.ParseString(encoder.Encode(*event)));
  ASSERT_EQ(parsed_log.GetNumberOfEvents(), 1u);
  ASSERT_EQ(parsed_log.GetEventType(0),
            ParsedRtcEventLog::BWE_PROBE_RESULT_EVENT);

  auto parsed_event = parsed_log.GetBweProbeResult(0);

  EXPECT_EQ(parsed_log.GetTimestamp(0), timestamp_us);
  EXPECT_EQ(rtc::dchecked_cast<int>(parsed_event.id), id);
  ASSERT_TRUE(parsed_event.bitrate_bps);
  EXPECT_EQ(parsed_event.bitrate_bps, bitrate_bps);
  ASSERT_FALSE(parsed_event.failure_reason);
}

TEST_F(RtcEventLogEncoderLegacyTest, RtcEventRtcpPacketIncoming) {}

TEST_F(RtcEventLogEncoderLegacyTest, RtcEventRtcpPacketOutgoing) {}

TEST_F(RtcEventLogEncoderLegacyTest, RtcEventRtpPacketIncoming) {}

TEST_F(RtcEventLogEncoderLegacyTest, RtcEventRtpPacketOutgoing) {}

TEST_F(RtcEventLogEncoderLegacyTest, RtcEventVideoReceiveStreamConfig) {}

TEST_F(RtcEventLogEncoderLegacyTest, RtcEventVideoSendStreamConfig) {}

}  // namespace webrtc
