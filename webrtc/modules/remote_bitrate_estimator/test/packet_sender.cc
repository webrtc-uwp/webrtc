/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/remote_bitrate_estimator/test/packet_sender.h"

#include <algorithm>
#include <list>
#include <sstream>

#include "webrtc/base/checks.h"
#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/modules/remote_bitrate_estimator/test/bwe.h"

namespace webrtc {
namespace testing {
namespace bwe {

std::list<FeedbackPacket*> GetFeedbackPackets(Packets* in_out,
                                              int64_t end_time_ms,
                                              int flow_id) {
  std::list<FeedbackPacket*> fb_packets;
  for (auto it = in_out->begin(); it != in_out->end();) {
    if ((*it)->send_time_us() > 1000 * end_time_ms)
      break;
    if ((*it)->GetPacketType() == Packet::kFeedback &&
        flow_id == (*it)->flow_id()) {
      fb_packets.push_back(static_cast<FeedbackPacket*>(*it));
      it = in_out->erase(it);
    } else {
      ++it;
    }
  }
  return fb_packets;
}

VideoSender::VideoSender(PacketProcessorListener* listener,
                         VideoSource* source,
                         BandwidthEstimatorType estimator_type)
    : PacketSender(listener, source->flow_id()),
      // For Packet::send_time_us() to be comparable with timestamps from
      // clock_, the clock of the VideoSender and the Source must be aligned.
      // We assume that both start at time 0.
      clock_(0),
      source_(source),
      bwe_(CreateBweSender(estimator_type,
                           source_->bits_per_second() / 1000,
                           this,
                           &clock_)) {
  modules_.push_back(bwe_.get());
}

VideoSender::~VideoSender() {
}

void VideoSender::RunFor(int64_t time_ms, Packets* in_out) {
  int64_t now_ms = clock_.TimeInMilliseconds();
  std::list<FeedbackPacket*> feedbacks =
      GetFeedbackPackets(in_out, now_ms + time_ms, source_->flow_id());
  ProcessFeedbackAndGeneratePackets(time_ms, &feedbacks, in_out);
}

void VideoSender::ProcessFeedbackAndGeneratePackets(
    int64_t time_ms,
    std::list<FeedbackPacket*>* feedbacks,
    Packets* packets) {
  do {
    // Make sure to at least run Process() below every 100 ms.
    int64_t time_to_run_ms = std::min<int64_t>(time_ms, 100);
    if (!feedbacks->empty()) {
      int64_t time_until_feedback_ms =
          feedbacks->front()->send_time_us() / 1000 -
          clock_.TimeInMilliseconds();
      time_to_run_ms =
          std::max<int64_t>(std::min(time_ms, time_until_feedback_ms), 0);
    }
    Packets generated;
    source_->RunFor(time_to_run_ms, &generated);
    bwe_->OnPacketsSent(generated);
    packets->merge(generated, DereferencingComparator<Packet>);
    clock_.AdvanceTimeMilliseconds(time_to_run_ms);
    if (!feedbacks->empty()) {
      bwe_->GiveFeedback(*feedbacks->front());
      delete feedbacks->front();
      feedbacks->pop_front();
    }
    bwe_->Process();
    time_ms -= time_to_run_ms;
  } while (time_ms > 0);
  assert(feedbacks->empty());
}

int VideoSender::GetFeedbackIntervalMs() const {
  return bwe_->GetFeedbackIntervalMs();
}

void VideoSender::OnNetworkChanged(uint32_t target_bitrate_bps,
                                   uint8_t fraction_lost,
                                   int64_t rtt) {
  source_->SetBitrateBps(target_bitrate_bps);
}

PacedVideoSender::PacedVideoSender(PacketProcessorListener* listener,
                                   VideoSource* source,
                                   BandwidthEstimatorType estimator)
    : VideoSender(listener, source, estimator),
      pacer_(&clock_,
             this,
             source->bits_per_second() / 1000,
             PacedSender::kDefaultPaceMultiplier * source->bits_per_second() /
                 1000,
             0) {
  modules_.push_back(&pacer_);
}

PacedVideoSender::~PacedVideoSender() {
  for (Packet* packet : pacer_queue_)
    delete packet;
  for (Packet* packet : queue_)
    delete packet;
}

void PacedVideoSender::RunFor(int64_t time_ms, Packets* in_out) {
  int64_t end_time_ms = clock_.TimeInMilliseconds() + time_ms;
  // Run process periodically to allow the packets to be paced out.
  std::list<FeedbackPacket*> feedbacks =
      GetFeedbackPackets(in_out, end_time_ms, source_->flow_id());
  int64_t last_run_time_ms = -1;
  BWE_TEST_LOGGING_CONTEXT("Sender");
  BWE_TEST_LOGGING_CONTEXT(source_->flow_id());
  do {
    int64_t time_until_process_ms = TimeUntilNextProcess(modules_);
    int64_t time_until_feedback_ms = time_ms;
    if (!feedbacks.empty())
      time_until_feedback_ms = feedbacks.front()->send_time_us() / 1000 -
                               clock_.TimeInMilliseconds();

    int64_t time_until_next_event_ms =
        std::min(time_until_feedback_ms, time_until_process_ms);

    time_until_next_event_ms =
        std::min(source_->GetTimeUntilNextFrameMs(), time_until_next_event_ms);

    // Never run for longer than we have been asked for.
    if (clock_.TimeInMilliseconds() + time_until_next_event_ms > end_time_ms)
      time_until_next_event_ms = end_time_ms - clock_.TimeInMilliseconds();

    // Make sure we don't get stuck if an event doesn't trigger. This typically
    // happens if the prober wants to probe, but there's no packet to send.
    if (time_until_next_event_ms == 0 && last_run_time_ms == 0)
      time_until_next_event_ms = 1;
    last_run_time_ms = time_until_next_event_ms;

    Packets generated_packets;
    source_->RunFor(time_until_next_event_ms, &generated_packets);
    if (!generated_packets.empty()) {
      for (Packet* packet : generated_packets) {
        MediaPacket* media_packet = static_cast<MediaPacket*>(packet);
        pacer_.SendPacket(PacedSender::kNormalPriority,
                          media_packet->header().ssrc,
                          media_packet->header().sequenceNumber,
                          (media_packet->send_time_us() + 500) / 1000,
                          media_packet->payload_size(), false);
        pacer_queue_.push_back(packet);
        assert(pacer_queue_.size() < 10000);
      }
    }

    clock_.AdvanceTimeMilliseconds(time_until_next_event_ms);

    if (time_until_next_event_ms == time_until_feedback_ms) {
      if (!feedbacks.empty()) {
        bwe_->GiveFeedback(*feedbacks.front());
        delete feedbacks.front();
        feedbacks.pop_front();
      }
      bwe_->Process();
    }

    if (time_until_next_event_ms == time_until_process_ms) {
      CallProcess(modules_);
    }
  } while (clock_.TimeInMilliseconds() < end_time_ms);
  QueuePackets(in_out, end_time_ms * 1000);
}

int64_t PacedVideoSender::TimeUntilNextProcess(
    const std::list<Module*>& modules) {
  int64_t time_until_next_process_ms = 10;
  for (Module* module : modules) {
    int64_t next_process_ms = module->TimeUntilNextProcess();
    if (next_process_ms < time_until_next_process_ms)
      time_until_next_process_ms = next_process_ms;
  }
  if (time_until_next_process_ms < 0)
    time_until_next_process_ms = 0;
  return time_until_next_process_ms;
}

void PacedVideoSender::CallProcess(const std::list<Module*>& modules) {
  for (Module* module : modules) {
    if (module->TimeUntilNextProcess() <= 0) {
      module->Process();
    }
  }
}

void PacedVideoSender::QueuePackets(Packets* batch,
                                    int64_t end_of_batch_time_us) {
  queue_.merge(*batch, DereferencingComparator<Packet>);
  if (queue_.empty()) {
    return;
  }
  Packets::iterator it = queue_.begin();
  for (; it != queue_.end(); ++it) {
    if ((*it)->send_time_us() > end_of_batch_time_us) {
      break;
    }
  }
  Packets to_transfer;
  to_transfer.splice(to_transfer.begin(), queue_, queue_.begin(), it);
  bwe_->OnPacketsSent(to_transfer);
  batch->merge(to_transfer, DereferencingComparator<Packet>);
}

bool PacedVideoSender::TimeToSendPacket(uint32_t ssrc,
                                        uint16_t sequence_number,
                                        int64_t capture_time_ms,
                                        bool retransmission) {
  for (Packets::iterator it = pacer_queue_.begin(); it != pacer_queue_.end();
       ++it) {
    MediaPacket* media_packet = static_cast<MediaPacket*>(*it);
    if (media_packet->header().sequenceNumber == sequence_number) {
      int64_t pace_out_time_ms = clock_.TimeInMilliseconds();
      // Make sure a packet is never paced out earlier than when it was put into
      // the pacer.
      assert(pace_out_time_ms >= (media_packet->send_time_us() + 500) / 1000);
      media_packet->SetAbsSendTimeMs(pace_out_time_ms);
      media_packet->set_send_time_us(1000 * pace_out_time_ms);
      queue_.push_back(media_packet);
      pacer_queue_.erase(it);
      return true;
    }
  }
  return false;
}

size_t PacedVideoSender::TimeToSendPadding(size_t bytes) {
  return 0;
}

void PacedVideoSender::OnNetworkChanged(uint32_t target_bitrate_bps,
                                        uint8_t fraction_lost,
                                        int64_t rtt) {
  VideoSender::OnNetworkChanged(target_bitrate_bps, fraction_lost, rtt);
  pacer_.UpdateBitrate(
      target_bitrate_bps / 1000,
      PacedSender::kDefaultPaceMultiplier * target_bitrate_bps / 1000, 0);
}

void TcpSender::RunFor(int64_t time_ms, Packets* in_out) {
  BWE_TEST_LOGGING_CONTEXT("Sender");
  BWE_TEST_LOGGING_CONTEXT(*flow_ids().begin());
  std::list<FeedbackPacket*> feedbacks =
      GetFeedbackPackets(in_out, now_ms_ + time_ms, *flow_ids().begin());
  // The number of packets which are sent in during time_ms depends on the
  // number of packets in_flight_ and the max number of packets in flight
  // (cwnd_). Therefore SendPackets() isn't directly dependent on time_ms.
  for (FeedbackPacket* fb : feedbacks) {
    UpdateCongestionControl(fb);
    SendPackets(in_out);
  }
  SendPackets(in_out);
  now_ms_ += time_ms;
}

void TcpSender::SendPackets(Packets* in_out) {
  int cwnd = ceil(cwnd_);
  int packets_to_send = std::max(cwnd - in_flight_, 0);
  if (packets_to_send > 0) {
    Packets generated = GeneratePackets(packets_to_send);
    in_flight_ += static_cast<int>(generated.size());
    in_out->merge(generated, DereferencingComparator<Packet>);
  }
}

void TcpSender::UpdateCongestionControl(const FeedbackPacket* fb) {
  const TcpFeedback* tcp_fb = static_cast<const TcpFeedback*>(fb);
  DCHECK(!tcp_fb->acked_packets().empty());
  ack_received_ = true;

  in_flight_ -= static_cast<int>(tcp_fb->acked_packets().size());
  DCHECK_GE(in_flight_, 0);

  if (LossEvent(tcp_fb->acked_packets())) {
    cwnd_ /= 2.0f;
    in_slow_start_ = false;
  } else if (in_slow_start_) {
    cwnd_ += tcp_fb->acked_packets().size();
  } else {
    cwnd_ += 1.0f / cwnd_;
  }

  last_acked_seq_num_ =
      LatestSequenceNumber(tcp_fb->acked_packets().back(), last_acked_seq_num_);
}

bool TcpSender::LossEvent(const std::vector<uint16_t>& acked_packets) {
  int missing = 0;
  for (int i = last_acked_seq_num_ + 1; i <= acked_packets.back(); ++i) {
    if (std::find(acked_packets.begin(), acked_packets.end(), i) ==
        acked_packets.end()) {
      ++missing;
    }
  }
  in_flight_ -= missing;
  return missing > 0;
}

Packets TcpSender::GeneratePackets(size_t num_packets) {
  Packets generated;
  for (size_t i = 0; i < num_packets; ++i) {
    generated.push_back(new MediaPacket(*flow_ids().begin(), 1000 * now_ms_,
                                        1200, next_sequence_number_++));
  }
  return generated;
}
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
