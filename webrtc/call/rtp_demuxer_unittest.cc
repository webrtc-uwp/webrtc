/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/call/rtp_demuxer.h"

#include <memory>
#include <set>
#include <string>

#include "webrtc/call/ssrc_binding_observer.h"
#include "webrtc/call/test/mock_rtp_packet_sink_interface.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_packet_received.h"
#include "webrtc/rtc_base/arraysize.h"
#include "webrtc/rtc_base/basictypes.h"
#include "webrtc/rtc_base/checks.h"
#include "webrtc/rtc_base/ptr_util.h"
#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"

namespace webrtc {

namespace {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::AtMost;
using ::testing::InSequence;
using ::testing::MockFunction;
using ::testing::NiceMock;

class MockSsrcBindingObserver : public SsrcBindingObserver {
 public:
  MOCK_METHOD2(OnSsrcBoundToRsid, void(const std::string& rsid, uint32_t ssrc));
  MOCK_METHOD2(OnSsrcBoundToMid, void(const std::string& mid, uint32_t ssrc));
  MOCK_METHOD3(OnSsrcBoundToMidRsid,
               void(const std::string& mid,
                    const std::string& rsid,
                    uint32_t ssrc));
  MOCK_METHOD2(OnSsrcBoundToPayloadType,
               void(uint8_t payload_type, uint32_t ssrc));
};

MATCHER_P(SamePacketAs, other, "") {
  return arg.Ssrc() == other.Ssrc() &&
         arg.SequenceNumber() == other.SequenceNumber();
}

std::unique_ptr<RtpPacketReceived> CreateRtpPacketReceived(
    uint32_t ssrc,
    size_t sequence_number = 0,
    RtpPacketReceived::ExtensionManager* extension_manager = nullptr) {
  // |sequence_number| is declared |size_t| to prevent ugly casts when calling
  // the function, but should in reality always be a |uint16_t|.
  EXPECT_LT(sequence_number, 1u << 16);

  auto packet = rtc::MakeUnique<RtpPacketReceived>(extension_manager);
  packet->SetSsrc(ssrc);
  packet->SetSequenceNumber(static_cast<uint16_t>(sequence_number));
  return packet;
}

std::unique_ptr<RtpPacketReceived> CreateRtpPacketReceivedWithMid(
    const std::string& mid,
    uint32_t ssrc,
    size_t sequence_number = 0) {
  RtpPacketReceived::ExtensionManager extension_manager;
  extension_manager.Register<RtpMid>(0xb);

  auto packet =
      CreateRtpPacketReceived(ssrc, sequence_number, &extension_manager);
  packet->SetExtension<RtpMid>(mid);
  return packet;
}

std::unique_ptr<RtpPacketReceived> CreateRtpPacketReceivedWithRsid(
    const std::string& rsid,
    uint32_t ssrc,
    size_t sequence_number = 0) {
  RtpPacketReceived::ExtensionManager extension_manager;
  extension_manager.Register<RtpStreamId>(0x6);

  auto packet =
      CreateRtpPacketReceived(ssrc, sequence_number, &extension_manager);
  packet->SetExtension<RtpStreamId>(rsid);
  return packet;
}

std::unique_ptr<RtpPacketReceived> CreateRtpPacketReceivedWithRepairedRsid(
    const std::string& rrsid,
    uint32_t ssrc,
    size_t sequence_number = 0) {
  RtpPacketReceived::ExtensionManager extension_manager;
  extension_manager.Register<RepairedRtpStreamId>(0x7);

  auto packet =
      CreateRtpPacketReceived(ssrc, sequence_number, &extension_manager);
  packet->SetExtension<RepairedRtpStreamId>(rrsid);
  return packet;
}

std::unique_ptr<RtpPacketReceived> CreateRtpPacketReceivedWithMidRsid(
    const std::string& mid,
    const std::string& rsid,
    uint32_t ssrc,
    size_t sequence_number = 0) {
  RtpPacketReceived::ExtensionManager extension_manager;
  extension_manager.Register<RtpMid>(0xb);
  extension_manager.Register<RtpStreamId>(0x6);

  auto packet =
      CreateRtpPacketReceived(ssrc, sequence_number, &extension_manager);
  packet->SetExtension<RtpMid>(mid);
  packet->SetExtension<RtpStreamId>(rsid);
  return packet;
}

class RtpDemuxerTest : public testing::Test {
 protected:
  ~RtpDemuxerTest() {
    for (auto* sink : sinks_to_tear_down_) {
      demuxer.RemoveSink(sink);
    }
    for (auto* observer : observers_to_tear_down_) {
      demuxer.DeregisterSsrcBindingObserver(observer);
    }
  }

  bool AddSink(const RtpDemuxerCriteria& criteria,
               RtpPacketSinkInterface* sink) {
    bool added = demuxer.AddSink(criteria, sink);
    if (added) {
      sinks_to_tear_down_.insert(sink);
    }
    return added;
  }

  bool AddSinkOnlySsrc(uint32_t ssrc, RtpPacketSinkInterface* sink) {
    RtpDemuxerCriteria criteria;
    criteria.ssrcs.push_back(ssrc);
    return AddSink(criteria, sink);
  }

  bool AddSinkOnlyRsid(const std::string& rsid, RtpPacketSinkInterface* sink) {
    RtpDemuxerCriteria criteria;
    criteria.rsid = rsid;
    return AddSink(criteria, sink);
  }

  bool AddSinkOnlyMid(const std::string& mid, RtpPacketSinkInterface* sink) {
    RtpDemuxerCriteria criteria;
    criteria.mid = mid;
    return AddSink(criteria, sink);
  }

  bool AddSinkBothMidRsid(const std::string& mid,
                          const std::string& rsid,
                          RtpPacketSinkInterface* sink) {
    RtpDemuxerCriteria criteria;
    criteria.mid = mid;
    criteria.rsid = rsid;
    return AddSink(criteria, sink);
  }

  bool RemoveSink(RtpPacketSinkInterface* sink) {
    DoNotTearDownSink(sink);
    return demuxer.RemoveSink(sink);
  }

  void DoNotTearDownSink(RtpPacketSinkInterface* sink) {
    sinks_to_tear_down_.erase(sink);
  }

  void RegisterSsrcBindingObserver(SsrcBindingObserver* observer) {
    demuxer.RegisterSsrcBindingObserver(observer);
    observers_to_tear_down_.insert(observer);
  }

  void DeregisterSsrcBindingObserver(SsrcBindingObserver* observer) {
    demuxer.DeregisterSsrcBindingObserver(observer);
    DoNotTearDownObserver(observer);
  }

  void DoNotTearDownObserver(SsrcBindingObserver* observer) {
    observers_to_tear_down_.erase(observer);
  }

  RtpDemuxer demuxer;
  std::set<RtpPacketSinkInterface*> sinks_to_tear_down_;
  std::set<SsrcBindingObserver*> observers_to_tear_down_;
};

// Helper macros.

// Example usage:
// MockRtpPacketSink sink;
// EXPECT_SINK_RECEIVE(sink).Times(1)
#define EXPECT_SINK_RECEIVE(sink) EXPECT_CALL(sink, OnRtpPacket(_))

// Example usage:
// MockRtpPacketSink sink;
// auto packet = CreateRtpPacketReceived(...);
// EXPECT_SINK_RECEIVE_PACKET(sink, packet).Times(1);
#define EXPECT_SINK_RECEIVE_PACKET(sink, packet) \
  EXPECT_CALL(sink, OnRtpPacket(SamePacketAs(*packet)))

#define EXPECT_DELIVER(packet) EXPECT_TRUE(demuxer.OnRtpPacket(*packet))

#define EXPECT_DROP(packet) EXPECT_FALSE(demuxer.OnRtpPacket(*packet))

TEST_F(RtpDemuxerTest, CanAddSinkBySsrc) {
  MockRtpPacketSink sink;
  constexpr uint32_t ssrc = 1;

  EXPECT_TRUE(AddSinkOnlySsrc(ssrc, &sink));
}

TEST_F(RtpDemuxerTest, OnRtpPacketCalledOnCorrectSinkBySsrc) {
  constexpr uint32_t ssrcs[] = {101, 202, 303};
  MockRtpPacketSink sinks[arraysize(ssrcs)];
  for (size_t i = 0; i < arraysize(ssrcs); i++) {
    AddSinkOnlySsrc(ssrcs[i], &sinks[i]);
  }

  for (size_t i = 0; i < arraysize(ssrcs); i++) {
    auto packet = CreateRtpPacketReceived(ssrcs[i]);
    EXPECT_SINK_RECEIVE_PACKET(sinks[i], packet).Times(1);
    EXPECT_DELIVER(packet);
  }
}

TEST_F(RtpDemuxerTest, OnRtpPacketCalledOnCorrectSinkByRsid) {
  const std::string rsids[] = {"a", "b", "c"};
  MockRtpPacketSink sinks[arraysize(rsids)];
  for (size_t i = 0; i < arraysize(rsids); i++) {
    AddSinkOnlyRsid(rsids[i], &sinks[i]);
  }

  for (size_t i = 0; i < arraysize(rsids); i++) {
    auto packet =
        CreateRtpPacketReceivedWithRsid(rsids[i], static_cast<uint32_t>(i), i);
    EXPECT_SINK_RECEIVE_PACKET(sinks[i], packet).Times(1);
    EXPECT_DELIVER(packet);
  }
}

TEST_F(RtpDemuxerTest, OnRtpPacketCalledOnCorrectSinkByMid) {
  const std::string mids[] = {"a", "b", "c"};
  MockRtpPacketSink sinks[arraysize(mids)];
  for (size_t i = 0; i < arraysize(mids); i++) {
    AddSinkOnlyMid(mids[i], &sinks[i]);
  }

  for (size_t i = 0; i < arraysize(mids); i++) {
    auto packet =
        CreateRtpPacketReceivedWithMid(mids[i], static_cast<uint32_t>(i), i);
    EXPECT_SINK_RECEIVE_PACKET(sinks[i], packet).Times(1);
    EXPECT_DELIVER(packet);
  }
}

TEST_F(RtpDemuxerTest, PacketsDeliveredInRightOrder) {
  constexpr uint32_t ssrc = 101;
  MockRtpPacketSink sink;
  AddSinkOnlySsrc(ssrc, &sink);

  std::unique_ptr<RtpPacketReceived> packets[5];
  for (size_t i = 0; i < arraysize(packets); i++) {
    packets[i] = CreateRtpPacketReceived(ssrc, i);
  }

  InSequence sequence;
  for (const auto& packet : packets) {
    EXPECT_SINK_RECEIVE_PACKET(sink, packet).Times(1);
  }

  for (const auto& packet : packets) {
    EXPECT_DELIVER(packet);
  }
}

TEST_F(RtpDemuxerTest, SinkMappedToMultipleSsrcs) {
  constexpr uint32_t ssrcs[] = {404, 505, 606};
  MockRtpPacketSink sink;
  for (uint32_t ssrc : ssrcs) {
    AddSinkOnlySsrc(ssrc, &sink);
  }

  // The sink which is associated with multiple SSRCs gets the callback
  // triggered for each of those SSRCs.
  for (uint32_t ssrc : ssrcs) {
    auto packet = CreateRtpPacketReceived(ssrc);
    EXPECT_SINK_RECEIVE_PACKET(sink, packet);
    EXPECT_DELIVER(packet);
  }
}

TEST_F(RtpDemuxerTest, NoCallbackOnSsrcSinkRemovedBeforeFirstPacket) {
  constexpr uint32_t ssrc = 404;
  MockRtpPacketSink sink;
  AddSinkOnlySsrc(ssrc, &sink);

  ASSERT_TRUE(demuxer.RemoveSink(&sink));

  // The removed sink does not get callbacks.
  auto packet = CreateRtpPacketReceived(ssrc);
  EXPECT_SINK_RECEIVE(sink).Times(0);
  EXPECT_DROP(packet);
}

TEST_F(RtpDemuxerTest, NoCallbackOnSsrcSinkRemovedAfterFirstPacket) {
  constexpr uint32_t ssrc = 404;
  NiceMock<MockRtpPacketSink> sink;
  AddSinkOnlySsrc(ssrc, &sink);

  InSequence sequence;
  uint16_t seq_num;
  for (seq_num = 0; seq_num < 10; seq_num++) {
    ASSERT_TRUE(demuxer.OnRtpPacket(*CreateRtpPacketReceived(ssrc, seq_num)));
  }

  ASSERT_TRUE(RemoveSink(&sink));

  // The removed sink does not get callbacks.
  auto packet = CreateRtpPacketReceived(ssrc, seq_num);
  EXPECT_SINK_RECEIVE(sink).Times(0);
  EXPECT_DROP(packet);
}

TEST_F(RtpDemuxerTest, AddSinkFailsIfCalledForTwoSinks) {
  MockRtpPacketSink sink_a;
  MockRtpPacketSink sink_b;
  constexpr uint32_t ssrc = 1;
  ASSERT_TRUE(AddSinkOnlySsrc(ssrc, &sink_a));

  EXPECT_FALSE(AddSinkOnlySsrc(ssrc, &sink_b));
}

// An SSRC may only be mapped to a single sink. However, since configuration
// of this associations might come from the network, we need to fail gracefully.
TEST_F(RtpDemuxerTest, OnlyOneSinkPerSsrcGetsOnRtpPacketTriggered) {
  MockRtpPacketSink sinks[3];
  constexpr uint32_t ssrc = 404;
  ASSERT_TRUE(AddSinkOnlySsrc(ssrc, &sinks[0]));
  ASSERT_FALSE(AddSinkOnlySsrc(ssrc, &sinks[1]));
  ASSERT_FALSE(AddSinkOnlySsrc(ssrc, &sinks[2]));

  // The first sink associated with the SSRC remains active; other sinks
  // were not really added, and so do not get OnRtpPacket() called.
  auto packet = CreateRtpPacketReceived(ssrc);
  EXPECT_SINK_RECEIVE_PACKET(sinks[0], packet);
  EXPECT_DELIVER(packet);
}

TEST_F(RtpDemuxerTest, AddSinkFailsIfCalledTwiceEvenIfSameSink) {
  MockRtpPacketSink sink;
  constexpr uint32_t ssrc = 1;
  ASSERT_TRUE(AddSinkOnlySsrc(ssrc, &sink));

  EXPECT_FALSE(AddSinkOnlySsrc(ssrc, &sink));
}

TEST_F(RtpDemuxerTest, NoRepeatedCallbackOnRepeatedAddSinkForSameSink) {
  constexpr uint32_t ssrc = 111;
  MockRtpPacketSink sink;

  ASSERT_TRUE(AddSinkOnlySsrc(ssrc, &sink));
  ASSERT_FALSE(AddSinkOnlySsrc(ssrc, &sink));

  auto packet = CreateRtpPacketReceived(ssrc);
  EXPECT_SINK_RECEIVE_PACKET(sink, packet).Times(1);
  EXPECT_DELIVER(packet);
}

TEST_F(RtpDemuxerTest, RemoveSinkReturnsFalseForNeverAddedSink) {
  MockRtpPacketSink sink;

  EXPECT_FALSE(demuxer.RemoveSink(&sink));
}

TEST_F(RtpDemuxerTest, RemoveSinkReturnsTrueForPreviouslyAddedSsrcSink) {
  constexpr uint32_t ssrc = 101;
  MockRtpPacketSink sink;
  AddSinkOnlySsrc(ssrc, &sink);

  EXPECT_TRUE(RemoveSink(&sink));
}

TEST_F(RtpDemuxerTest,
       RemoveSinkReturnsTrueForUnresolvedPreviouslyAddedRsidSink) {
  const std::string rsid = "a";
  MockRtpPacketSink sink;
  AddSinkOnlyRsid(rsid, &sink);

  EXPECT_TRUE(RemoveSink(&sink));
}

TEST_F(RtpDemuxerTest,
       RemoveSinkReturnsTrueForResolvedPreviouslyAddedRsidSink) {
  const std::string rsid = "a";
  constexpr uint32_t ssrc = 101;
  NiceMock<MockRtpPacketSink> sink;
  AddSinkOnlyRsid(rsid, &sink);
  EXPECT_DELIVER(CreateRtpPacketReceivedWithRsid(rsid, ssrc));

  EXPECT_TRUE(RemoveSink(&sink));
}

TEST_F(RtpDemuxerTest, OnRtpPacketCalledForRsidSink) {
  MockRtpPacketSink sink;
  const std::string rsid = "a";
  AddSinkOnlyRsid(rsid, &sink);

  // Create a sequence of RTP packets, where only the first one actually
  // mentions the RSID.
  std::unique_ptr<RtpPacketReceived> packets[5];
  constexpr uint32_t rsid_ssrc = 111;
  packets[0] = CreateRtpPacketReceivedWithRsid(rsid, rsid_ssrc);
  for (size_t i = 1; i < arraysize(packets); i++) {
    packets[i] = CreateRtpPacketReceived(rsid_ssrc, i);
  }

  // The first packet associates the RSID with the SSRC, thereby allowing the
  // demuxer to correctly demux all of the packets.
  InSequence sequence;
  for (const auto& packet : packets) {
    EXPECT_SINK_RECEIVE_PACKET(sink, packet).Times(1);
  }
  for (const auto& packet : packets) {
    EXPECT_DELIVER(packet);
  }
}

TEST_F(RtpDemuxerTest, NoCallbackOnRsidSinkRemovedBeforeFirstPacket) {
  MockRtpPacketSink sink;
  const std::string rsid = "a";
  AddSinkOnlyRsid(rsid, &sink);

  // Sink removed - it won't get triggers even if packets with its RSID arrive.
  ASSERT_TRUE(RemoveSink(&sink));

  constexpr uint32_t ssrc = 111;
  auto packet = CreateRtpPacketReceivedWithRsid(rsid, ssrc);
  EXPECT_SINK_RECEIVE(sink).Times(0);  // Not called.
  EXPECT_DROP(packet);
}

TEST_F(RtpDemuxerTest, NoCallbackOnRsidSinkRemovedAfterFirstPacket) {
  NiceMock<MockRtpPacketSink> sink;
  const std::string rsid = "a";
  AddSinkOnlyRsid(rsid, &sink);

  InSequence sequence;
  constexpr uint32_t ssrc = 111;
  uint16_t seq_num;
  for (seq_num = 0; seq_num < 10; seq_num++) {
    auto packet = CreateRtpPacketReceivedWithRsid(rsid, ssrc, seq_num);
    ASSERT_TRUE(demuxer.OnRtpPacket(*packet));
  }

  // Sink removed - it won't get triggers even if packets with its RSID arrive.
  ASSERT_TRUE(RemoveSink(&sink));

  auto packet = CreateRtpPacketReceivedWithRsid(rsid, ssrc, seq_num);
  EXPECT_SINK_RECEIVE(sink).Times(0);  // Not called.
  EXPECT_DROP(packet);
}

// The RSID to SSRC mapping should be one-to-one. If we end up receiving
// two (or more) packets with the same SSRC, but different RSIDs, we guarantee
// delivery to one of them but not both.
TEST_F(RtpDemuxerTest, FirstSsrcAssociatedWithAnRsidIsNotForgotten) {
  // Each sink has a distinct RSID.
  MockRtpPacketSink sink_a;
  const std::string rsid_a = "a";
  AddSinkOnlyRsid(rsid_a, &sink_a);

  MockRtpPacketSink sink_b;
  const std::string rsid_b = "b";
  AddSinkOnlyRsid(rsid_b, &sink_b);

  InSequence sequence;  // Verify that the order of delivery is unchanged.

  constexpr uint32_t shared_ssrc = 100;

  // First a packet with |rsid_a| is received, and |sink_a| is associated with
  // its SSRC.
  auto packet_a = CreateRtpPacketReceivedWithRsid(rsid_a, shared_ssrc, 10);
  EXPECT_SINK_RECEIVE_PACKET(sink_a, packet_a).Times(1);
  EXPECT_DELIVER(packet_a);

  // Second, a packet with |rsid_b| is received. We guarantee that just one of
  // the sinks receives it.
  auto packet_b = CreateRtpPacketReceivedWithRsid(rsid_b, shared_ssrc, 20);

  MockFunction<void()> check_point1;
  EXPECT_SINK_RECEIVE_PACKET(sink_a, packet_b)
      .Times(AtMost(1))
      .WillRepeatedly(
          InvokeWithoutArgs(&check_point1, &MockFunction<void()>::Call));
  EXPECT_SINK_RECEIVE_PACKET(sink_b, packet_b)
      .Times(AtMost(1))
      .WillRepeatedly(
          InvokeWithoutArgs(&check_point1, &MockFunction<void()>::Call));
  EXPECT_CALL(check_point1, Call()).Times(1);

  EXPECT_DELIVER(packet_b);

  // Known edge-case; adding a new RSID association makes us re-examine all
  // SSRCs. |sink_b| may or may not be associated with the SSRC now; we make
  // no promises on that. We do however still guarantee that exactly one of the
  // sinks receives the new packets.
  MockRtpPacketSink sink_c;
  const std::string rsid_c = "c";
  constexpr uint32_t some_other_ssrc = shared_ssrc + 1;
  AddSinkOnlySsrc(some_other_ssrc, &sink_c);

  auto packet_c = CreateRtpPacketReceivedWithRsid(rsid_c, shared_ssrc, 30);

  MockFunction<void()> check_point2;
  EXPECT_SINK_RECEIVE_PACKET(sink_a, packet_c)
      .Times(AtMost(1))
      .WillRepeatedly(
          InvokeWithoutArgs(&check_point2, &MockFunction<void()>::Call));
  EXPECT_SINK_RECEIVE_PACKET(sink_b, packet_c)
      .Times(AtMost(1))
      .WillRepeatedly(
          InvokeWithoutArgs(&check_point2, &MockFunction<void()>::Call));
  EXPECT_CALL(check_point2, Call()).Times(1);

  EXPECT_DELIVER(packet_c);
}

TEST_F(RtpDemuxerTest, MultipleRsidsOnSameSink) {
  MockRtpPacketSink sink;
  const std::string rsids[] = {"a", "b", "c"};

  for (const std::string& rsid : rsids) {
    AddSinkOnlyRsid(rsid, &sink);
  }

  InSequence sequence;
  for (size_t i = 0; i < arraysize(rsids); i++) {
    // Assign different SSRCs and sequence numbers to all packets.
    const uint32_t ssrc = 1000 + static_cast<uint32_t>(i);
    const uint16_t sequence_number = 50 + static_cast<uint16_t>(i);
    auto packet =
        CreateRtpPacketReceivedWithRsid(rsids[i], ssrc, sequence_number);
    EXPECT_SINK_RECEIVE_PACKET(sink, packet).Times(1);
    EXPECT_DELIVER(packet);
  }
}

TEST_F(RtpDemuxerTest, SinkWithBothRsidAndSsrcAssociations) {
  MockRtpPacketSink sink;
  constexpr uint32_t standalone_ssrc = 10101;
  constexpr uint32_t rsid_ssrc = 20202;
  const std::string rsid = "a";

  AddSinkOnlySsrc(standalone_ssrc, &sink);
  AddSinkOnlyRsid(rsid, &sink);

  InSequence sequence;

  auto ssrc_packet = CreateRtpPacketReceived(standalone_ssrc, 11);
  EXPECT_SINK_RECEIVE_PACKET(sink, ssrc_packet).Times(1);
  EXPECT_DELIVER(ssrc_packet);

  auto rsid_packet = CreateRtpPacketReceivedWithRsid(rsid, rsid_ssrc, 22);
  EXPECT_SINK_RECEIVE_PACKET(sink, rsid_packet).Times(1);
  EXPECT_DELIVER(rsid_packet);
}

TEST_F(RtpDemuxerTest, AssociatingByRsidAndBySsrcCannotTriggerDoubleCall) {
  MockRtpPacketSink sink;

  constexpr uint32_t ssrc = 10101;
  AddSinkOnlySsrc(ssrc, &sink);

  const std::string rsid = "a";
  AddSinkOnlyRsid(rsid, &sink);

  constexpr uint16_t seq_num = 999;
  auto packet = CreateRtpPacketReceivedWithRsid(rsid, ssrc, seq_num);
  EXPECT_SINK_RECEIVE_PACKET(sink, packet).Times(1);
  EXPECT_DELIVER(packet);
}

TEST_F(RtpDemuxerTest, RsidObserversInformedOfResolutionsOfTrackedRsids) {
  constexpr uint32_t ssrc = 111;
  const std::string rsid = "a";

  // Only RSIDs which the demuxer knows may be resolved.
  NiceMock<MockRtpPacketSink> sink;
  AddSinkOnlyRsid(rsid, &sink);

  NiceMock<MockSsrcBindingObserver> rsid_resolution_observers[3];
  for (auto& observer : rsid_resolution_observers) {
    RegisterSsrcBindingObserver(&observer);
  }

  // The expected calls to OnSsrcBoundToRsid() will be triggered by this.
  demuxer.OnRtpPacket(*CreateRtpPacketReceivedWithRsid(rsid, ssrc));
}

TEST_F(RtpDemuxerTest, RsidObserversNotInformedOfResolutionsOfUntrackedRsids) {
  constexpr uint32_t ssrc = 111;
  const std::string rsid = "a";

  MockSsrcBindingObserver rsid_resolution_observers[3];
  for (auto& observer : rsid_resolution_observers) {
    RegisterSsrcBindingObserver(&observer);
    EXPECT_CALL(observer, OnSsrcBoundToRsid(rsid, ssrc)).Times(0);
  }

  // The expected calls to OnRsidResolved() will be triggered by this.
  demuxer.OnRtpPacket(*CreateRtpPacketReceivedWithRsid(rsid, ssrc));
}

// If one sink is associated with SSRC x, and another sink with RSID y, we
// should never observe RSID x being resolved to SSRC x, or else we'd end
// up with one SSRC mapped to two sinks. However, if such faulty input
// ever reaches us, we should handle it gracefully - not crash.
TEST_F(RtpDemuxerTest,
       PacketFittingBothRsidSinkAndSsrcSinkGivenOnlyToSsrcSink) {
  constexpr uint32_t ssrc = 111;
  MockRtpPacketSink ssrc_sink;
  AddSinkOnlySsrc(ssrc, &ssrc_sink);

  const std::string rsid = "a";
  MockRtpPacketSink rsid_sink;
  AddSinkOnlyRsid(rsid, &rsid_sink);

  MockFunction<void()> check_point;

  auto packet = CreateRtpPacketReceivedWithRsid(rsid, ssrc);

  // Either sink may receive the packet but not both.
  EXPECT_SINK_RECEIVE_PACKET(ssrc_sink, packet)
      .Times(AtMost(1))
      .WillRepeatedly(
          InvokeWithoutArgs(&check_point, &MockFunction<void()>::Call));
  EXPECT_SINK_RECEIVE_PACKET(rsid_sink, packet)
      .Times(AtMost(1))
      .WillRepeatedly(
          InvokeWithoutArgs(&check_point, &MockFunction<void()>::Call));
  EXPECT_CALL(check_point, Call()).Times(1);

  EXPECT_DELIVER(packet);
}

TEST_F(RtpDemuxerTest,
       PacketFittingBothRsidSinkAndSsrcSinkMayTriggerResolutionCallbacks) {
  constexpr uint32_t ssrc = 111;
  NiceMock<MockRtpPacketSink> ssrc_sink;
  AddSinkOnlySsrc(ssrc, &ssrc_sink);

  const std::string rsid = "a";
  NiceMock<MockRtpPacketSink> rsid_sink;
  AddSinkOnlyRsid(rsid, &rsid_sink);

  MockSsrcBindingObserver observer;
  RegisterSsrcBindingObserver(&observer);

  auto packet = CreateRtpPacketReceivedWithRsid(rsid, ssrc);
  EXPECT_CALL(observer, OnSsrcBoundToRsid(_, _)).Times(AtMost(1));
  demuxer.OnRtpPacket(*packet);
}

// We're not expecting RSIDs to be resolved to SSRCs which were previously
// mapped to sinks, and make no guarantees except for graceful handling.
TEST_F(RtpDemuxerTest,
       GracefullyHandleRsidBeingMappedToPrevouslyAssociatedSsrc) {
  constexpr uint32_t ssrc = 111;
  NiceMock<MockRtpPacketSink> ssrc_sink;
  AddSinkOnlySsrc(ssrc, &ssrc_sink);

  const std::string rsid = "a";
  NiceMock<MockRtpPacketSink> rsid_sink;
  AddSinkOnlyRsid(rsid, &rsid_sink);

  NiceMock<MockSsrcBindingObserver> observer;
  RegisterSsrcBindingObserver(&observer);

  // The SSRC was mapped to an SSRC sink, but was even active (packets flowed
  // over it).
  auto packet = CreateRtpPacketReceivedWithRsid(rsid, ssrc);
  demuxer.OnRtpPacket(*packet);

  // If the SSRC sink is ever removed, the RSID sink *might* receive indications
  // of packets, and observers *might* be informed. Only graceful handling
  // is guaranteed.
  RemoveSink(&ssrc_sink);
  EXPECT_SINK_RECEIVE_PACKET(rsid_sink, packet).Times(AtLeast(0));
  EXPECT_CALL(observer, OnSsrcBoundToRsid(rsid, ssrc)).Times(AtLeast(0));
  EXPECT_DELIVER(packet);
}

TEST_F(RtpDemuxerTest, DeregisteredRsidObserversNotInformedOfResolutions) {
  constexpr uint32_t ssrc = 111;
  const std::string rsid = "a";
  NiceMock<MockRtpPacketSink> sink;
  AddSinkOnlyRsid(rsid, &sink);

  // Register several, then deregister only one, to show that not all of the
  // observers had been forgotten when one was removed.
  MockSsrcBindingObserver observer_1;
  MockSsrcBindingObserver observer_2_removed;
  MockSsrcBindingObserver observer_3;

  RegisterSsrcBindingObserver(&observer_1);
  RegisterSsrcBindingObserver(&observer_2_removed);
  RegisterSsrcBindingObserver(&observer_3);

  DeregisterSsrcBindingObserver(&observer_2_removed);

  EXPECT_CALL(observer_1, OnSsrcBoundToRsid(rsid, ssrc)).Times(1);
  EXPECT_CALL(observer_2_removed, OnSsrcBoundToRsid(_, _)).Times(0);
  EXPECT_CALL(observer_3, OnSsrcBoundToRsid(rsid, ssrc)).Times(1);

  // The expected calls to OnSsrcBoundToRsid() will be triggered by this.
  EXPECT_DELIVER(CreateRtpPacketReceivedWithRsid(rsid, ssrc));
}

TEST_F(RtpDemuxerTest, PacketRoutedByMid) {
  const std::string mid = "mid";
  MockRtpPacketSink sink;
  AddSinkOnlyMid(mid, &sink);

  auto packet = CreateRtpPacketReceivedWithMid(mid, 10, 1);
  EXPECT_SINK_RECEIVE_PACKET(sink, packet).Times(1);
  EXPECT_DELIVER(packet);
}

// Tests that when one MID sink is configured, packets that include the MID
// extension will get routed to that sink and any packets that use the same
// SSRC as one of those packets later will also get routed to the sink, even
// if a new SSRC is introduced for the same MID.
TEST_F(RtpDemuxerTest, RoutedByMidWhenSsrcAdded) {
  const std::string mid = "mid";
  NiceMock<MockRtpPacketSink> sink;
  AddSinkOnlyMid(mid, &sink);

  constexpr uint32_t ssrc1 = 10;
  constexpr uint32_t ssrc2 = 11;

  auto p1 = CreateRtpPacketReceivedWithMid(mid, ssrc1, 1);
  EXPECT_SINK_RECEIVE_PACKET(sink, p1).Times(1);
  EXPECT_DELIVER(p1);

  auto p2 = CreateRtpPacketReceivedWithMid(mid, ssrc2, 2);
  EXPECT_SINK_RECEIVE_PACKET(sink, p2).Times(1);
  EXPECT_DELIVER(p2);

  auto p3 = CreateRtpPacketReceived(ssrc1, 3);
  EXPECT_SINK_RECEIVE_PACKET(sink, p3).Times(1);
  EXPECT_DELIVER(p3);

  auto p4 = CreateRtpPacketReceived(ssrc2, 4);
  EXPECT_SINK_RECEIVE_PACKET(sink, p4).Times(1);
  EXPECT_DELIVER(p4);
}

TEST_F(RtpDemuxerTest, DontLearnMidSsrcBindingBeforeSinkAdded) {
  const std::string mid = "mid";
  constexpr uint32_t ssrc = 10;

  auto p1 = CreateRtpPacketReceivedWithMid(mid, ssrc, 1);
  EXPECT_DROP(p1);

  MockRtpPacketSink sink;
  AddSinkOnlyMid(mid, &sink);

  auto p2 = CreateRtpPacketReceived(ssrc, 2);
  EXPECT_SINK_RECEIVE_PACKET(sink, p2).Times(0);
  EXPECT_DROP(p2);
}

TEST_F(RtpDemuxerTest, DontForgetMidSsrcBindingWhenSinkRemoved) {
  const std::string mid = "v";
  constexpr uint32_t ssrc = 10;

  MockRtpPacketSink sink1;
  AddSinkOnlyMid(mid, &sink1);

  auto packet_with_mid = CreateRtpPacketReceivedWithMid(mid, ssrc, 1);
  EXPECT_SINK_RECEIVE_PACKET(sink1, packet_with_mid);
  EXPECT_DELIVER(packet_with_mid);

  RemoveSink(&sink1);

  MockRtpPacketSink sink2;
  AddSinkOnlyMid(mid, &sink2);

  auto packet_with_ssrc = CreateRtpPacketReceived(ssrc, 2);
  EXPECT_SINK_RECEIVE_PACKET(sink2, packet_with_ssrc);
  EXPECT_DELIVER(packet_with_ssrc);
}

TEST_F(RtpDemuxerTest, RejectAddSinkForSameMidOnly) {
  const std::string mid = "mid";

  MockRtpPacketSink sink;
  AddSinkOnlyMid(mid, &sink);
  EXPECT_FALSE(AddSinkOnlyMid(mid, &sink));
}

TEST_F(RtpDemuxerTest, RoutedByMidWithAnyRsid) {
  const std::string mid = "v";

  const std::string rsid1 = "1";
  const std::string rsid2 = "2";

  constexpr uint32_t ssrc1 = 10;
  constexpr uint32_t ssrc2 = 11;

  MockRtpPacketSink sink;
  AddSinkOnlyMid(mid, &sink);

  EXPECT_SINK_RECEIVE(sink).Times(2);

  auto p1 = CreateRtpPacketReceivedWithMidRsid(mid, rsid1, ssrc1, 1);
  auto p2 = CreateRtpPacketReceivedWithMidRsid(mid, rsid2, ssrc2, 2);

  EXPECT_DELIVER(p1);
  EXPECT_DELIVER(p2);
}

TEST_F(RtpDemuxerTest, NotRoutedByMidIfSinkRemovedImmediately) {
  const std::string mid = "v";
  constexpr uint32_t ssrc = 10;

  MockRtpPacketSink sink;
  AddSinkOnlyMid(mid, &sink);
  RemoveSink(&sink);

  EXPECT_SINK_RECEIVE(sink).Times(0);

  auto packet = CreateRtpPacketReceivedWithMid(mid, ssrc, 1);
  EXPECT_DROP(packet);
}

TEST_F(RtpDemuxerTest, NotRoutedByMidIfSinkRemovedAfterBinding) {
  const std::string mid = "v";
  constexpr uint32_t ssrc = 10;

  MockRtpPacketSink sink;
  AddSinkOnlyMid(mid, &sink);

  auto p1 = CreateRtpPacketReceivedWithMid(mid, ssrc, 1);
  EXPECT_SINK_RECEIVE(sink).Times(AnyNumber());
  EXPECT_DELIVER(p1);

  RemoveSink(&sink);

  auto p2 = CreateRtpPacketReceivedWithMid(mid, ssrc, 2);
  EXPECT_SINK_RECEIVE_PACKET(sink, p2).Times(0);
  EXPECT_DROP(p2);
}

TEST_F(RtpDemuxerTest, RoutedByMidRsidInSamePacket) {
  const std::string mid = "v";
  const std::string rsid = "1";

  MockRtpPacketSink sink;
  AddSinkBothMidRsid(mid, rsid, &sink);

  auto packet = CreateRtpPacketReceivedWithMidRsid(mid, rsid, 10, 1);
  EXPECT_SINK_RECEIVE_PACKET(sink, packet).Times(1);
  EXPECT_DELIVER(packet);
}

TEST_F(RtpDemuxerTest, LearnMidThenRsidSeparatelyAndRouteBySsrc) {
  const std::string mid = "v";
  const std::string rsid = "1";
  constexpr uint32_t ssrc = 10;

  MockRtpPacketSink sink;
  AddSinkBothMidRsid(mid, rsid, &sink);

  auto packet_with_mid = CreateRtpPacketReceivedWithMid(mid, ssrc, 1);
  EXPECT_SINK_RECEIVE_PACKET(sink, packet_with_mid).Times(0);
  EXPECT_DROP(packet_with_mid);

  auto packet_with_rsid = CreateRtpPacketReceivedWithRsid(rsid, ssrc, 2);
  EXPECT_SINK_RECEIVE_PACKET(sink, packet_with_rsid).Times(1);
  EXPECT_DELIVER(packet_with_rsid);

  auto packet_with_ssrc = CreateRtpPacketReceived(ssrc, 3);
  EXPECT_SINK_RECEIVE_PACKET(sink, packet_with_ssrc).Times(1);
  EXPECT_DELIVER(packet_with_ssrc);
}

TEST_F(RtpDemuxerTest, LearnRsidThenMidSeparatelyAndRouteBySsrc) {
  const std::string mid = "v";
  const std::string rsid = "1";
  constexpr uint32_t ssrc = 10;

  MockRtpPacketSink sink;
  AddSinkBothMidRsid(mid, rsid, &sink);

  auto packet_with_rsid = CreateRtpPacketReceivedWithRsid(rsid, ssrc, 1);
  EXPECT_SINK_RECEIVE_PACKET(sink, packet_with_rsid).Times(0);
  EXPECT_DROP(packet_with_rsid);

  auto packet_with_mid = CreateRtpPacketReceivedWithMid(mid, ssrc, 2);
  EXPECT_SINK_RECEIVE_PACKET(sink, packet_with_mid).Times(1);
  EXPECT_DELIVER(packet_with_mid);

  auto packet_with_ssrc = CreateRtpPacketReceived(ssrc, 3);
  EXPECT_SINK_RECEIVE_PACKET(sink, packet_with_ssrc).Times(1);
  EXPECT_DELIVER(packet_with_ssrc);
}

TEST_F(RtpDemuxerTest, DontLearnMidRsidBindingBeforeSinkAdded) {
  const std::string mid = "v";
  const std::string rsid = "1";
  constexpr uint32_t ssrc = 10;

  auto packet_with_both =
      CreateRtpPacketReceivedWithMidRsid(mid, rsid, ssrc, 1);
  EXPECT_DROP(packet_with_both);

  MockRtpPacketSink sink;
  AddSinkBothMidRsid(mid, rsid, &sink);

  auto packet_with_ssrc = CreateRtpPacketReceived(ssrc, 2);
  EXPECT_SINK_RECEIVE_PACKET(sink, packet_with_ssrc).Times(0);
  EXPECT_DROP(packet_with_ssrc);
}

TEST_F(RtpDemuxerTest, DontForgetMidRsidBindingWhenSinkRemoved) {
  const std::string mid = "v";
  const std::string rsid = "1";
  constexpr uint32_t ssrc = 10;

  MockRtpPacketSink sink1;
  AddSinkBothMidRsid(mid, rsid, &sink1);

  auto packet_with_both =
      CreateRtpPacketReceivedWithMidRsid(mid, rsid, ssrc, 1);
  EXPECT_SINK_RECEIVE_PACKET(sink1, packet_with_both);
  EXPECT_DELIVER(packet_with_both);

  RemoveSink(&sink1);

  MockRtpPacketSink sink2;
  AddSinkBothMidRsid(mid, rsid, &sink2);

  auto packet_with_ssrc = CreateRtpPacketReceived(ssrc, 2);
  EXPECT_SINK_RECEIVE_PACKET(sink2, packet_with_ssrc);
  EXPECT_DELIVER(packet_with_ssrc);
}

TEST_F(RtpDemuxerTest, LearnMidRsidBindingAfterSinkAdded) {
  const std::string mid = "v";
  const std::string rsid = "1";
  constexpr uint32_t ssrc = 10;

  MockRtpPacketSink sink;
  AddSinkBothMidRsid(mid, rsid, &sink);

  auto packet_with_both =
      CreateRtpPacketReceivedWithMidRsid(mid, rsid, ssrc, 1);
  EXPECT_SINK_RECEIVE_PACKET(sink, packet_with_both);
  EXPECT_DELIVER(packet_with_both);

  auto packet_with_ssrc = CreateRtpPacketReceived(ssrc, 2);
  EXPECT_SINK_RECEIVE_PACKET(sink, packet_with_ssrc);
  EXPECT_DELIVER(packet_with_ssrc);
}

TEST_F(RtpDemuxerTest, RejectAddSinkForSameMidRsid) {
  const std::string mid = "v";
  const std::string rsid = "1";

  MockRtpPacketSink sink1;
  AddSinkBothMidRsid(mid, rsid, &sink1);

  MockRtpPacketSink sink2;
  EXPECT_FALSE(AddSinkBothMidRsid(mid, rsid, &sink2));
}

TEST_F(RtpDemuxerTest, RouteByRtpRepairStream) {
  const std::string rsid = "1";
  constexpr uint32_t ssrc = 10;

  MockRtpPacketSink sink;
  AddSinkOnlyRsid(rsid, &sink);

  auto packet_with_rrid =
      CreateRtpPacketReceivedWithRepairedRsid(rsid, ssrc, 1);
  EXPECT_SINK_RECEIVE_PACKET(sink, packet_with_rrid);
  EXPECT_DELIVER(packet_with_rrid);
}

TEST_F(RtpDemuxerTest, DroppedWithMidRsidIfMidRsidSinkRemoved) {
  const std::string mid = "v";
  const std::string rsid = "1";
  constexpr uint32_t ssrc = 10;

  MockRtpPacketSink sink;
  AddSinkBothMidRsid(mid, rsid, &sink);

  auto p1 = CreateRtpPacketReceivedWithMidRsid(mid, rsid, ssrc, 1);
  EXPECT_SINK_RECEIVE_PACKET(sink, p1).Times(1);
  EXPECT_DELIVER(p1);

  RemoveSink(&sink);

  auto p2 = CreateRtpPacketReceivedWithMidRsid(mid, rsid, ssrc, 2);
  EXPECT_SINK_RECEIVE_PACKET(sink, p2).Times(0);
  EXPECT_DROP(p2);
}

TEST_F(RtpDemuxerTest, DropByPayloadTypeIfNoSink) {
  constexpr uint8_t payload_type = 30;
  constexpr uint32_t ssrc = 10;

  auto packet = CreateRtpPacketReceived(ssrc, 1);
  packet->SetPayloadType(payload_type);
  EXPECT_DROP(packet);
}

TEST_F(RtpDemuxerTest, RouteByPayloadTypeIfOneSink) {
  constexpr uint8_t payload_type = 30;
  constexpr uint32_t ssrc = 10;

  RtpDemuxerCriteria pt;
  pt.payload_types = {payload_type};
  MockRtpPacketSink sink;
  AddSink(pt, &sink);

  auto packet = CreateRtpPacketReceived(ssrc, 1);
  packet->SetPayloadType(payload_type);
  EXPECT_SINK_RECEIVE_PACKET(sink, packet).Times(1);
  EXPECT_DELIVER(packet);
}

TEST_F(RtpDemuxerTest, DropByPayloadTypeIfMultipleSinks) {
  constexpr uint8_t payload_type = 30;
  constexpr uint32_t ssrc = 10;

  RtpDemuxerCriteria pt;
  pt.payload_types = {payload_type};

  MockRtpPacketSink sink1;
  AddSink(pt, &sink1);

  MockRtpPacketSink sink2;
  AddSink(pt, &sink2);

  EXPECT_SINK_RECEIVE(sink1).Times(0);
  EXPECT_SINK_RECEIVE(sink2).Times(0);

  auto packet = CreateRtpPacketReceived(ssrc, 1);
  packet->SetPayloadType(payload_type);
  EXPECT_DROP(packet);
}

TEST_F(RtpDemuxerTest, DontRejectAddSinkForOverlappingPayloadTypes) {
  constexpr uint8_t pt1 = 30;
  constexpr uint8_t pt2 = 31;
  constexpr uint8_t pt3 = 32;

  RtpDemuxerCriteria pt1_pt2;
  pt1_pt2.payload_types = {pt1, pt2};
  MockRtpPacketSink sink1;
  AddSink(pt1_pt2, &sink1);

  RtpDemuxerCriteria pt1_pt3;
  pt1_pt3.payload_types = {pt1, pt3};
  MockRtpPacketSink sink2;
  EXPECT_TRUE(AddSink(pt1_pt3, &sink2));
}

TEST_F(RtpDemuxerTest, RoutedByPayloadTypeIfAmbiguousSinkRemoved) {
  constexpr uint8_t payload_type = 30;
  constexpr uint32_t ssrc = 10;

  RtpDemuxerCriteria pt;
  pt.payload_types = {payload_type};

  MockRtpPacketSink sink1;
  AddSink(pt, &sink1);

  MockRtpPacketSink sink2;
  AddSink(pt, &sink2);

  RemoveSink(&sink1);

  auto packet = CreateRtpPacketReceived(ssrc, 1);
  packet->SetPayloadType(payload_type);

  EXPECT_SINK_RECEIVE_PACKET(sink1, packet).Times(0);
  EXPECT_SINK_RECEIVE_PACKET(sink2, packet).Times(1);

  EXPECT_DELIVER(packet);
}

TEST_F(RtpDemuxerTest, RoutedByPayloadTypeLatchesSsrc) {
  constexpr uint8_t payload_type = 30;
  constexpr uint32_t ssrc = 10;

  RtpDemuxerCriteria pt;
  pt.payload_types = {payload_type};
  MockRtpPacketSink sink;
  AddSink(pt, &sink);

  auto packet_with_pt = CreateRtpPacketReceived(ssrc, 1);
  packet_with_pt->SetPayloadType(payload_type);
  EXPECT_SINK_RECEIVE_PACKET(sink, packet_with_pt).Times(1);
  EXPECT_DELIVER(packet_with_pt);

  auto packet_with_ssrc = CreateRtpPacketReceived(ssrc, 1);
  EXPECT_SINK_RECEIVE_PACKET(sink, packet_with_ssrc).Times(1);
  EXPECT_DELIVER(packet_with_ssrc);
}

// RSIDs are scoped within MID, so if two sinks are registered with the same
// RSIDs but different MIDs, then packets containing both extensions should be
// routed to the correct one.
TEST_F(RtpDemuxerTest, PacketWithSameRsidDifferentMidRoutedToProperSink) {
  const std::string mid1 = "mid1";
  const std::string mid2 = "mid2";
  const std::string rsid = "rsid";

  MockRtpPacketSink mid1_sink;
  AddSinkBothMidRsid(mid1, rsid, &mid1_sink);

  MockRtpPacketSink mid2_sink;
  AddSinkBothMidRsid(mid2, rsid, &mid2_sink);

  auto packet_mid1 = CreateRtpPacketReceivedWithMidRsid(mid1, rsid, 11, 1);
  EXPECT_SINK_RECEIVE_PACKET(mid1_sink, packet_mid1).Times(1);
  EXPECT_DELIVER(packet_mid1);

  auto packet_mid2 = CreateRtpPacketReceivedWithMidRsid(mid2, rsid, 12, 2);
  EXPECT_SINK_RECEIVE_PACKET(mid2_sink, packet_mid2).Times(1);
  EXPECT_DELIVER(packet_mid2);
}

// If a sink is first bound to a given SSRC by signaling but later a new sink is
// bound to a given MID by a later signaling, then when a packet arrives with
// both the SSRC and MID, then the signaled MID sink should take precedence.
TEST_F(RtpDemuxerTest, SignaledMidShouldOverwriteSignaledSsrc) {
  constexpr uint32_t ssrc = 11;
  const std::string mid = "mid";

  MockRtpPacketSink ssrc_sink;
  AddSinkOnlySsrc(ssrc, &ssrc_sink);

  MockRtpPacketSink mid_sink;
  AddSinkOnlyMid(mid, &mid_sink);

  auto p = CreateRtpPacketReceivedWithMid(mid, ssrc, 1);
  EXPECT_SINK_RECEIVE(ssrc_sink).Times(0);
  EXPECT_SINK_RECEIVE(mid_sink).Times(1);
  EXPECT_DELIVER(p);
}

// Extends the previous test to also ensure that later packets that do not
// specify MID are still routed to the MID sink rather than the overwritten SSRC
// sink.
TEST_F(RtpDemuxerTest, SignaledMidShouldOverwriteSignalledSsrcPersistent) {
  constexpr uint32_t ssrc = 11;
  const std::string mid = "mid";

  MockRtpPacketSink ssrc_sink;
  AddSinkOnlySsrc(ssrc, &ssrc_sink);

  MockRtpPacketSink mid_sink;
  AddSinkOnlyMid(mid, &mid_sink);

  EXPECT_SINK_RECEIVE(ssrc_sink).Times(0);
  EXPECT_SINK_RECEIVE(mid_sink).Times(2);

  auto packet_with_mid = CreateRtpPacketReceivedWithMid(mid, ssrc, 1);
  EXPECT_DELIVER(packet_with_mid);
  auto packet_without_mid = CreateRtpPacketReceived(ssrc, 2);
  EXPECT_DELIVER(packet_without_mid);
}

TEST_F(RtpDemuxerTest, RouteByPayloadType) {
  constexpr uint8_t payload_type = 10;

  MockRtpPacketSink sink;
  RtpDemuxerCriteria criteria;
  criteria.payload_types = {payload_type};
  AddSink(criteria, &sink);

  auto packet = CreateRtpPacketReceived(128, 1);
  packet->SetPayloadType(payload_type);
  EXPECT_SINK_RECEIVE(sink).Times(1);
  EXPECT_DELIVER(packet);
}

TEST_F(RtpDemuxerTest, RouteByPayloadTypeMultipleMatch) {
  constexpr uint8_t pt1 = 10;
  constexpr uint8_t pt2 = 11;

  MockRtpPacketSink sink;
  RtpDemuxerCriteria criteria;
  criteria.payload_types = {pt1, pt2};
  AddSink(criteria, &sink);
}

TEST_F(RtpDemuxerTest, DontRouteIfPayloadTypeAmbiguous) {
  constexpr uint8_t payload_type = 10;

  MockRtpPacketSink sink1;
  MockRtpPacketSink sink2;
  RtpDemuxerCriteria criteria;
  criteria.payload_types = {payload_type};

  AddSink(criteria, &sink1);
  EXPECT_TRUE(AddSink(criteria, &sink2));

  EXPECT_SINK_RECEIVE(sink1).Times(0);
  EXPECT_SINK_RECEIVE(sink2).Times(0);

  auto packet = CreateRtpPacketReceived(128, 1);
  packet->SetPayloadType(payload_type);
  EXPECT_DROP(packet);
}

/*TEST_F(RtpDemuxerTest, TestAB) {
  const std::string mid = "v";
  const std::string rsid = "1";

  MockRtpPacketSink sink;
  RtpDemuxerCriteria criteria;
  criteria.mid = mid;
  criteria.rsids = {rsid};
  AddSink(criteria, &sink);

  auto packet = CreateRtpPacketReceivedWithMid(mid, 10, 1);
  EXPECT_SINK_RECEIVE_PACKET(sink, packet);
  EXPECT_DELIVER(packet);
}*/

TEST_F(RtpDemuxerTest, ObserversNotifiedOfSsrcBoundToMid) {
  const std::string mid = "v";
  constexpr uint32_t ssrc = 10;

  NiceMock<MockRtpPacketSink> sink;
  AddSinkOnlyMid(mid, &sink);

  MockSsrcBindingObserver obs;
  RegisterSsrcBindingObserver(&obs);

  auto packet = CreateRtpPacketReceivedWithMid(mid, ssrc, 1);
  EXPECT_CALL(obs, OnSsrcBoundToMid(mid, ssrc));
  EXPECT_DELIVER(packet);
}

TEST_F(RtpDemuxerTest, ObserversNotifiedOfSsrcBoundToMidRsid) {
  const std::string mid = "v";
  const std::string rsid = "1";
  constexpr uint32_t ssrc = 10;

  NiceMock<MockRtpPacketSink> sink;
  AddSinkBothMidRsid(mid, rsid, &sink);

  MockSsrcBindingObserver obs;
  RegisterSsrcBindingObserver(&obs);

  auto packet = CreateRtpPacketReceivedWithMidRsid(mid, rsid, ssrc, 1);
  EXPECT_CALL(obs, OnSsrcBoundToMidRsid(mid, rsid, ssrc));
  EXPECT_DELIVER(packet);
}

TEST_F(RtpDemuxerTest, ObserversNotifiedOfSsrcBoundToPayloadType) {
  constexpr uint8_t payload_type = 3;
  constexpr uint32_t ssrc = 10;

  RtpDemuxerCriteria criteria;
  criteria.payload_types = {payload_type};
  NiceMock<MockRtpPacketSink> sink;
  AddSink(criteria, &sink);

  MockSsrcBindingObserver obs;
  RegisterSsrcBindingObserver(&obs);

  auto packet = CreateRtpPacketReceived(ssrc, 1);
  packet->SetPayloadType(payload_type);
  EXPECT_CALL(obs, OnSsrcBoundToPayloadType(payload_type, ssrc));
  EXPECT_DELIVER(packet);
}

TEST_F(RtpDemuxerTest, ObserversNotifiedOfSsrcBoundtoMidOnlyOnce) {
  const std::string mid = "v";
  constexpr uint32_t ssrc = 10;

  NiceMock<MockRtpPacketSink> sink;
  AddSinkOnlyMid(mid, &sink);

  MockSsrcBindingObserver obs;
  RegisterSsrcBindingObserver(&obs);

  EXPECT_CALL(obs, OnSsrcBoundToMid(mid, ssrc)).Times(1);

  auto p1 = CreateRtpPacketReceivedWithMid(mid, ssrc, 1);
  EXPECT_DELIVER(p1);

  auto p2 = CreateRtpPacketReceivedWithMid(mid, ssrc, 2);
  EXPECT_DELIVER(p2);
}

TEST_F(RtpDemuxerTest, ObserversNotifiedOfSsrcBoundtoMidWhenSsrcChanges) {
  const std::string mid = "v";
  constexpr uint32_t ssrc1 = 10;
  constexpr uint32_t ssrc2 = 11;

  NiceMock<MockRtpPacketSink> sink;
  AddSinkOnlyMid(mid, &sink);

  MockSsrcBindingObserver obs;
  RegisterSsrcBindingObserver(&obs);

  InSequence seq;
  EXPECT_CALL(obs, OnSsrcBoundToMid(mid, ssrc1));
  EXPECT_CALL(obs, OnSsrcBoundToMid(mid, ssrc2));

  auto p1 = CreateRtpPacketReceivedWithMid(mid, ssrc1, 1);
  EXPECT_DELIVER(p1);

  auto p2 = CreateRtpPacketReceivedWithMid(mid, ssrc2, 2);
  EXPECT_DELIVER(p2);
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
TEST_F(RtpDemuxerTest, CriteriaMustBeNonEmpty) {
  MockRtpPacketSink sink;
  RtpDemuxerCriteria criteria;

  EXPECT_DEATH(demuxer.AddSink(criteria, &sink), "");
}

TEST_F(RtpDemuxerTest, RsidMustBeNonEmpty) {
  MockRtpPacketSink sink;

  EXPECT_DEATH(demuxer.AddSink("", &sink), "");
}

TEST_F(RtpDemuxerTest, RsidMustBeAlphaNumeric) {
  MockRtpPacketSink sink;

  EXPECT_DEATH(demuxer.AddSink("a_3", &sink), "");
}

TEST_F(RtpDemuxerTest, RsidMustNotExceedMaximumLength) {
  MockRtpPacketSink sink;
  std::string rsid(StreamId::kMaxSize + 1, 'a');

  EXPECT_DEATH(demuxer.AddSink(rsid, &sink), "");
}

TEST_F(RtpDemuxerTest, DoubleRegisterationOfRsidResolutionObserverDisallowed) {
  MockSsrcBindingObserver observer;
  RegisterSsrcBindingObserver(&observer);

  EXPECT_DEATH(demuxer.RegisterSsrcBindingObserver(&observer), "");
}

TEST_F(RtpDemuxerTest,
       DregisterationOfNeverRegisteredRsidResolutionObserverDisallowed) {
  MockSsrcBindingObserver observer;

  EXPECT_DEATH(demuxer.DeregisterSsrcBindingObserver(&observer), "");
}

#endif

}  // namespace
}  // namespace webrtc
