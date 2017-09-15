/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/include/rtcp_transceiver.h"

#include "modules/rtp_rtcp/include/receive_statistics.h"
#include "modules/utility/include/process_thread.h"
#include "rtc_base/event.h"
#include "rtc_base/location.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/mock_transport.h"
#include "test/rtcp_packet_parser.h"

namespace {

using ::testing::AtLeast;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SizeIs;
using ::testing::_;
using ::webrtc::MockTransport;
using ::webrtc::RtcpTransceiver;
using ::webrtc::rtcp::ReportBlock;
using ::webrtc::test::RtcpPacketParser;

class MockReceiveStatisticsProvider : public webrtc::ReceiveStatisticsProvider {
 public:
  MOCK_METHOD1(RtcpReportBlocks, std::vector<ReportBlock>(size_t));
};

class RtcpTransceiverTest : public ::testing::Test {
 public:
  RtcpTransceiverTest()
      : process_thread_(webrtc::ProcessThread::Create("worker")) {
    process_thread_->Start();
  }
  ~RtcpTransceiverTest() override { process_thread_->Stop(); }

  webrtc::ProcessThread* thread() { return process_thread_.get(); }

 private:
  std::unique_ptr<webrtc::ProcessThread> process_thread_;
};

TEST_F(RtcpTransceiverTest, PeriodicallySendsReceiverReport) {
  const uint32_t kSenderSsrc = 1234;
  const uint32_t kMediaSsrc = 3456;
  rtc::Event success(false, false);
  size_t sent_rtcp_packets = 0;
  MockReceiveStatisticsProvider receive_statistics;
  std::vector<ReportBlock> report_blocks(1);
  report_blocks[0].SetMediaSsrc(kMediaSsrc);
  EXPECT_CALL(receive_statistics, RtcpReportBlocks(_))
      .WillRepeatedly(Return(report_blocks));

  MockTransport outgoing_transport;
  EXPECT_CALL(outgoing_transport, SendRtcp(_, _))
      .WillRepeatedly(Invoke([&](const uint8_t* buffer, size_t size) {
        RtcpPacketParser rtcp_parser;
        EXPECT_TRUE(rtcp_parser.Parse(buffer, size));
        EXPECT_EQ(rtcp_parser.receiver_report()->num_packets(), 1);
        EXPECT_EQ(rtcp_parser.receiver_report()->sender_ssrc(), kSenderSsrc);
        EXPECT_THAT(rtcp_parser.receiver_report()->report_blocks(),
                    SizeIs(report_blocks.size()));
        EXPECT_EQ(
            rtcp_parser.receiver_report()->report_blocks()[0].source_ssrc(),
            kMediaSsrc);
        if (++sent_rtcp_packets >= 2)  // i.e. EXPECT_CALL().Times(AtLeast(2))
          success.Set();
        return true;
      }));

  RtcpTransceiver::Configuration config;
  config.feedback_ssrc = kSenderSsrc;
  config.outgoing_transport = &outgoing_transport;
  config.receive_statistics = &receive_statistics;
  config.min_periodic_report_ms = 10;

  RtcpTransceiver rtcp(config);
  thread()->RegisterModule(&rtcp, RTC_FROM_HERE);

  EXPECT_TRUE(success.Wait(/*milliseconds=*/25));
}

TEST_F(RtcpTransceiverTest, ForceSendReportAsap) {
  rtc::Event success(false, false);
  MockTransport outgoing_transport;
  EXPECT_CALL(outgoing_transport, SendRtcp(_, _))
      .WillRepeatedly(Invoke([&](const uint8_t* buffer, size_t size) {
        RtcpPacketParser rtcp_parser;
        EXPECT_TRUE(rtcp_parser.Parse(buffer, size));
        EXPECT_GT(rtcp_parser.receiver_report()->num_packets(), 0);
        success.Set();
        return true;
      }));

  RtcpTransceiver::Configuration config;
  config.outgoing_transport = &outgoing_transport;
  config.min_periodic_report_ms = 10;
  RtcpTransceiver rtcp(config);
  thread()->RegisterModule(&rtcp, RTC_FROM_HERE);

  // Wait until first periodic report.
  EXPECT_TRUE(success.Wait(/*milliseconds=*/10));
  success.Reset();

  rtcp.ForceSendReport();

  // Wait as little as 1ms for forced report.
  EXPECT_TRUE(success.Wait(/*milliseconds=*/1));
}

TEST_F(RtcpTransceiverTest, AttachSdesWhenCnameSpecified) {
  const uint32_t kSenderSsrc = 1234;
  const std::string kCname = "sender";
  rtc::Event success(false, false);
  MockTransport outgoing_transport;
  EXPECT_CALL(outgoing_transport, SendRtcp(_, _))
      .WillRepeatedly(Invoke([&](const uint8_t* buffer, size_t size) {
        RtcpPacketParser rtcp_parser;
        EXPECT_TRUE(rtcp_parser.Parse(buffer, size));
        EXPECT_EQ(rtcp_parser.sdes()->num_packets(), 1);
        EXPECT_THAT(rtcp_parser.sdes()->chunks(), SizeIs(1));
        EXPECT_EQ(rtcp_parser.sdes()->chunks()[0].ssrc, kSenderSsrc);
        EXPECT_EQ(rtcp_parser.sdes()->chunks()[0].cname, kCname);
        success.Set();
        return true;
      }));

  RtcpTransceiver::Configuration config;
  config.feedback_ssrc = kSenderSsrc;
  config.cname = kCname;
  config.outgoing_transport = &outgoing_transport;
  config.min_periodic_report_ms = 10;
  RtcpTransceiver rtcp(config);
  thread()->RegisterModule(&rtcp, RTC_FROM_HERE);

  rtcp.ForceSendReport();
  EXPECT_TRUE(success.Wait(/*milliseconds=*/5));
}

}  // namespace
