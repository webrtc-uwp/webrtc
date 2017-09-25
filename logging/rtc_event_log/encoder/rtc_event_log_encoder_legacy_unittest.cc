/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <string>
#include <utility>

#include "api/rtpparameters.h"  // RtpExtension
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
#include "modules/audio_coding/audio_network_adaptor/include/audio_network_adaptor_config.h"
#include "modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtcp_packet/bye.h"  // Arbitrary RTCP message.
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/safe_conversions.h"
#include "test/gtest.h"

namespace webrtc {

namespace {
const char* const arbitrary_uri =  // Just a recognized URI.
    "urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id";

const uint8_t kTransmissionTimeOffsetExtensionId = 1;
const uint8_t kAbsoluteSendTimeExtensionId = 14;
const uint8_t kTransportSequenceNumberExtensionId = 13;
const uint8_t kAudioLevelExtensionId = 9;
const uint8_t kVideoRotationExtensionId = 5;

const uint8_t kExtensionIds[] = {
    kTransmissionTimeOffsetExtensionId, kAbsoluteSendTimeExtensionId,
    kTransportSequenceNumberExtensionId, kAudioLevelExtensionId,
    kVideoRotationExtensionId};
const RTPExtensionType kExtensionTypes[] = {
    RTPExtensionType::kRtpExtensionTransmissionTimeOffset,
    RTPExtensionType::kRtpExtensionAbsoluteSendTime,
    RTPExtensionType::kRtpExtensionTransportSequenceNumber,
    RTPExtensionType::kRtpExtensionAudioLevel,
    RTPExtensionType::kRtpExtensionVideoRotation};
static_assert(arraysize(kExtensionIds) == arraysize(kExtensionTypes),
              "Size mismatch.");
}  // namespace

class RtcEventLogEncoderLegacyTest : public testing::Test {
 protected:
  ~RtcEventLogEncoderLegacyTest() override = default;

  // ANA events have some optional fields, so we want to make sure that we get
  // correct behavior both when all of the values are there, as well as when
  // only some.
  void TestRtcEventAudioNetworkAdaptation(
      std::unique_ptr<AudioEncoderRuntimeConfig> runtime_config);

  // These help prevent code duplication between incoming/outgoing variants.
  void TestRtcEventRtcpPacket(PacketDirection direction);
  void TestRtcEventRtpPacket(PacketDirection direction);

  RtcEventLogEncoderLegacy encoder;
  ParsedRtcEventLog parsed_log;
  // TODO(eladalon): Consider using a random input for the value, to demonstrate
  // that encoding works correctly in general, and not only over those values
  // which we happened to choose.
};

void RtcEventLogEncoderLegacyTest::TestRtcEventAudioNetworkAdaptation(
    std::unique_ptr<AudioEncoderRuntimeConfig> runtime_config) {
  auto original_runtime_config = *runtime_config;
  auto event = rtc::MakeUnique<RtcEventAudioNetworkAdaptation>(
      std::move(runtime_config));
  const int64_t timestamp_us = event->timestamp_us_;

  ASSERT_TRUE(parsed_log.ParseString(encoder.Encode(*event)));
  ASSERT_EQ(parsed_log.GetNumberOfEvents(), 1u);
  ASSERT_EQ(parsed_log.GetEventType(0),
            ParsedRtcEventLog::AUDIO_NETWORK_ADAPTATION_EVENT);

  AudioEncoderRuntimeConfig parsed_runtime_config;
  parsed_log.GetAudioNetworkAdaptation(0, &parsed_runtime_config);

  EXPECT_EQ(parsed_log.GetTimestamp(0), timestamp_us);
  EXPECT_EQ(parsed_runtime_config, original_runtime_config);
}

TEST_F(RtcEventLogEncoderLegacyTest, RtcEventAudioNetworkAdaptationBitrate) {
  auto runtime_config = rtc::MakeUnique<AudioEncoderRuntimeConfig>();
  runtime_config->bitrate_bps = rtc::Optional<int>(39);
  TestRtcEventAudioNetworkAdaptation(std::move(runtime_config));
}

TEST_F(RtcEventLogEncoderLegacyTest,
       RtcEventAudioNetworkAdaptationFrameLength) {
  auto runtime_config = rtc::MakeUnique<AudioEncoderRuntimeConfig>();
  runtime_config->frame_length_ms = rtc::Optional<int>(87);
  TestRtcEventAudioNetworkAdaptation(std::move(runtime_config));
}

TEST_F(RtcEventLogEncoderLegacyTest, RtcEventAudioNetworkAdaptationPacketLoss) {
  auto runtime_config = rtc::MakeUnique<AudioEncoderRuntimeConfig>();
  runtime_config->uplink_packet_loss_fraction = rtc::Optional<float>(0.75);
  TestRtcEventAudioNetworkAdaptation(std::move(runtime_config));
}

TEST_F(RtcEventLogEncoderLegacyTest, RtcEventAudioNetworkAdaptationFec) {
  // The test might be trivially passing for one of the two boolean values, so
  // for safety's sake, we test both. (Parameterization would be overkill.)
  for (bool fec_enabled : {false, true}) {
    auto runtime_config = rtc::MakeUnique<AudioEncoderRuntimeConfig>();
    runtime_config->enable_fec = rtc::Optional<bool>(fec_enabled);
    TestRtcEventAudioNetworkAdaptation(std::move(runtime_config));
  }
}

TEST_F(RtcEventLogEncoderLegacyTest, RtcEventAudioNetworkAdaptationDtx) {
  // The test might be trivially passing for one of the two boolean values, so
  // for safety's sake, we test both. (Parameterization would be overkill.)
  for (bool dtx_enabled : {false, true}) {
    auto runtime_config = rtc::MakeUnique<AudioEncoderRuntimeConfig>();
    runtime_config->enable_dtx = rtc::Optional<bool>(dtx_enabled);
    TestRtcEventAudioNetworkAdaptation(std::move(runtime_config));
  }
}

TEST_F(RtcEventLogEncoderLegacyTest, RtcEventAudioNetworkAdaptationChannels) {
  // The test might be trivially passing for one of the two possible values, so
  // for safety's sake, we test both. (Parameterization would be overkill.)
  for (size_t channels : {1, 2}) {
    auto runtime_config = rtc::MakeUnique<AudioEncoderRuntimeConfig>();
    runtime_config->num_channels = rtc::Optional<size_t>(channels);
    TestRtcEventAudioNetworkAdaptation(std::move(runtime_config));
  }
}

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

TEST_F(RtcEventLogEncoderLegacyTest, RtcEventAudioReceiveStreamConfig) {
  auto stream_config = rtc::MakeUnique<rtclog::StreamConfig>();
  stream_config->local_ssrc = 41;
  stream_config->remote_ssrc = 42;
  stream_config->rtp_extensions.push_back(RtpExtension(arbitrary_uri, 1));

  auto original_stream_config = *stream_config;

  auto event = rtc::MakeUnique<RtcEventAudioReceiveStreamConfig>(
      std::move(stream_config));
  const int64_t timestamp_us = event->timestamp_us_;

  ASSERT_TRUE(parsed_log.ParseString(encoder.Encode(*event)));
  ASSERT_EQ(parsed_log.GetNumberOfEvents(), 1u);
  ASSERT_EQ(parsed_log.GetEventType(0),
            ParsedRtcEventLog::AUDIO_RECEIVER_CONFIG_EVENT);

  auto parsed_event = parsed_log.GetAudioReceiveConfig(0);
  EXPECT_EQ(parsed_log.GetTimestamp(0), timestamp_us);
  EXPECT_EQ(parsed_event, original_stream_config);
}

TEST_F(RtcEventLogEncoderLegacyTest, RtcEventAudioSendStreamConfig) {
  auto stream_config = rtc::MakeUnique<rtclog::StreamConfig>();
  stream_config->local_ssrc = 414;
  stream_config->rtp_extensions.push_back(RtpExtension(arbitrary_uri, 11));

  auto original_stream_config = *stream_config;

  auto event =
      rtc::MakeUnique<RtcEventAudioSendStreamConfig>(std::move(stream_config));
  const int64_t timestamp_us = event->timestamp_us_;

  ASSERT_TRUE(parsed_log.ParseString(encoder.Encode(*event)));
  ASSERT_EQ(parsed_log.GetNumberOfEvents(), 1u);
  ASSERT_EQ(parsed_log.GetEventType(0),
            ParsedRtcEventLog::AUDIO_SENDER_CONFIG_EVENT);

  auto parsed_event = parsed_log.GetAudioSendConfig(0);
  EXPECT_EQ(parsed_log.GetTimestamp(0), timestamp_us);
  EXPECT_EQ(parsed_event, original_stream_config);
}

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

void RtcEventLogEncoderLegacyTest::TestRtcEventRtcpPacket(
    PacketDirection direction) {
  rtcp::Bye bye_packet;  // Arbitrarily chosen RTCP packet type.
  bye_packet.SetReason("a man's reach should exceed his grasp");
  auto rtcp_packet = bye_packet.Build();

  std::unique_ptr<RtcEvent> event;
  if (direction == PacketDirection::kIncomingPacket) {
    event = rtc::MakeUnique<RtcEventRtcpPacketIncoming>(rtcp_packet);
  } else {
    event = rtc::MakeUnique<RtcEventRtcpPacketOutgoing>(rtcp_packet);
  }
  const int64_t timestamp_us = event->timestamp_us_;

  ASSERT_TRUE(parsed_log.ParseString(encoder.Encode(*event)));
  ASSERT_EQ(parsed_log.GetNumberOfEvents(), 1u);
  ASSERT_EQ(parsed_log.GetEventType(0), ParsedRtcEventLog::RTCP_EVENT);

  PacketDirection parsed_direction;
  uint8_t parsed_packet[IP_PACKET_SIZE];  // "Parsed" = after event-encoding.
  size_t parsed_packet_length;
  parsed_log.GetRtcpPacket(0, &parsed_direction, parsed_packet,
                           &parsed_packet_length);

  EXPECT_EQ(parsed_direction, direction);
  EXPECT_EQ(parsed_log.GetTimestamp(0), timestamp_us);
  ASSERT_EQ(parsed_packet_length, rtcp_packet.size());
  ASSERT_EQ(memcmp(parsed_packet, rtcp_packet.data(), parsed_packet_length), 0);
}

TEST_F(RtcEventLogEncoderLegacyTest, RtcEventRtcpPacketIncoming) {
  TestRtcEventRtcpPacket(PacketDirection::kIncomingPacket);
}

TEST_F(RtcEventLogEncoderLegacyTest, RtcEventRtcpPacketOutgoing) {
  TestRtcEventRtcpPacket(PacketDirection::kOutgoingPacket);
}

void RtcEventLogEncoderLegacyTest::TestRtcEventRtpPacket(
    PacketDirection direction) {
  constexpr int probe_cluster_id = 3;
  RtpHeaderExtensionMap extensions_map;
  for (uint32_t i = 0; i < arraysize(kExtensionIds); i++) {
    extensions_map.Register(kExtensionTypes[i], kExtensionIds[i]);
  }

  std::unique_ptr<RtpPacketReceived> packet_received;
  std::unique_ptr<RtpPacketToSend> packet_to_send;
  RtpPacket* packet;
  if (direction == PacketDirection::kIncomingPacket) {
    packet_received = rtc::MakeUnique<RtpPacketReceived>(&extensions_map);
    packet = packet_received.get();
  } else {
    packet_to_send = rtc::MakeUnique<RtpPacketToSend>(&extensions_map);
    packet = packet_to_send.get();
  }
  packet->SetSsrc(1003);
  packet->SetSequenceNumber(776);
  packet->SetPayloadSize(90);

  std::unique_ptr<RtcEvent> event;
  if (direction == PacketDirection::kIncomingPacket) {
    event = rtc::MakeUnique<RtcEventRtpPacketIncoming>(*packet_received);
  } else {
    event = rtc::MakeUnique<RtcEventRtpPacketOutgoing>(*packet_to_send,
                                                       probe_cluster_id);
  }
  const int64_t timestamp_us = event->timestamp_us_;

  ASSERT_TRUE(parsed_log.ParseString(encoder.Encode(*event)));
  ASSERT_EQ(parsed_log.GetNumberOfEvents(), 1u);
  ASSERT_EQ(parsed_log.GetEventType(0), ParsedRtcEventLog::RTP_EVENT);

  PacketDirection parsed_direction;
  uint8_t parsed_rtp_header[IP_PACKET_SIZE];
  size_t parsed_header_length;
  size_t parsed_total_length;
  // TODO(eladalon): !!! Validate the ExtensionsMap?
  parsed_log.GetRtpHeader(0, &parsed_direction, parsed_rtp_header,
                          &parsed_header_length, &parsed_total_length);

  EXPECT_EQ(parsed_log.GetTimestamp(0), timestamp_us);
  EXPECT_EQ(parsed_direction, direction);
  if (parsed_direction == PacketDirection::kOutgoingPacket) {
    // TODO(eladalon): !!! Verify probe_cluster_id.
  }
  EXPECT_EQ(memcmp(parsed_rtp_header, packet->data(), parsed_header_length), 0);
  EXPECT_EQ(parsed_header_length, packet->headers_size());
  EXPECT_EQ(parsed_total_length, packet->size());
}

TEST_F(RtcEventLogEncoderLegacyTest, RtcEventRtpPacketIncoming) {
  TestRtcEventRtpPacket(PacketDirection::kIncomingPacket);
}

TEST_F(RtcEventLogEncoderLegacyTest, RtcEventRtpPacketOutgoing) {
  TestRtcEventRtpPacket(PacketDirection::kOutgoingPacket);
}

TEST_F(RtcEventLogEncoderLegacyTest, RtcEventVideoReceiveStreamConfig) {
  auto stream_config = rtc::MakeUnique<rtclog::StreamConfig>();
  stream_config->local_ssrc = 4100;
  stream_config->remote_ssrc = 4200;
  stream_config->rtcp_mode = RtcpMode::kCompound;
  stream_config->remb = true;
  stream_config->rtp_extensions.push_back(RtpExtension(arbitrary_uri, 1));
  stream_config->codecs.emplace_back("CODEC", 122, 7);

  auto original_stream_config = *stream_config;

  auto event = rtc::MakeUnique<RtcEventVideoReceiveStreamConfig>(
      std::move(stream_config));
  const int64_t timestamp_us = event->timestamp_us_;

  ASSERT_TRUE(parsed_log.ParseString(encoder.Encode(*event)));
  ASSERT_EQ(parsed_log.GetNumberOfEvents(), 1u);
  ASSERT_EQ(parsed_log.GetEventType(0),
            ParsedRtcEventLog::VIDEO_RECEIVER_CONFIG_EVENT);

  auto parsed_event = parsed_log.GetVideoReceiveConfig(0);
  EXPECT_EQ(parsed_log.GetTimestamp(0), timestamp_us);
  EXPECT_EQ(parsed_event, original_stream_config);
}

TEST_F(RtcEventLogEncoderLegacyTest, RtcEventVideoSendStreamConfig) {
  auto stream_config = rtc::MakeUnique<rtclog::StreamConfig>();
  stream_config->local_ssrc = 4100;
  stream_config->rtp_extensions.push_back(RtpExtension(arbitrary_uri, 12));
  stream_config->codecs.emplace_back("CODEC", 120, 3);

  auto original_stream_config = *stream_config;

  auto event =
      rtc::MakeUnique<RtcEventVideoSendStreamConfig>(std::move(stream_config));
  const int64_t timestamp_us = event->timestamp_us_;

  ASSERT_TRUE(parsed_log.ParseString(encoder.Encode(*event)));
  ASSERT_EQ(parsed_log.GetNumberOfEvents(), 1u);
  ASSERT_EQ(parsed_log.GetEventType(0),
            ParsedRtcEventLog::VIDEO_SENDER_CONFIG_EVENT);

  auto parsed_event = parsed_log.GetVideoSendConfig(0)[0];
  EXPECT_EQ(parsed_log.GetTimestamp(0), timestamp_us);
  EXPECT_EQ(parsed_event, original_stream_config);
}

}  // namespace webrtc
