/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/direct_transport.h"

#include "call/call.h"
#include "rtc_base/ptr_util.h"
#include "system_wrappers/include/clock.h"
#include "test/single_threaded_task_queue.h"

namespace webrtc {
namespace test {

DirectTransport::DirectTransport(
    SingleThreadedTaskQueueForTesting* task_queue,
    Call* send_call,
    const std::map<uint8_t, MediaType>& payload_type_map)
    : DirectTransport(task_queue,
                      FakeNetworkPipe::Config(),
                      send_call,
                      payload_type_map,
                      std::unique_ptr<test::RtpFileWriter>()) {}

DirectTransport::DirectTransport(
    SingleThreadedTaskQueueForTesting* task_queue,
    const FakeNetworkPipe::Config& config,
    Call* send_call,
    const std::map<uint8_t, MediaType>& payload_type_map,
    std::unique_ptr<test::RtpFileWriter> rtp_file_writer)
    : DirectTransport(
          task_queue,
          config,
          send_call,
          std::unique_ptr<Demuxer>(new DemuxerImpl(payload_type_map)),
          std::move(rtp_file_writer)) {}

DirectTransport::DirectTransport(
    SingleThreadedTaskQueueForTesting* task_queue,
    const FakeNetworkPipe::Config& config,
    Call* send_call,
    std::unique_ptr<Demuxer> demuxer,
    std::unique_ptr<test::RtpFileWriter> rtp_file_writer)
    : send_call_(send_call),
      clock_(Clock::GetRealTimeClock()),
      task_queue_(task_queue),
      fake_network_(clock_, config, std::move(demuxer)),
      receiver_(nullptr),
      start_ms_(rtc::TimeMillis()),
      rtp_file_writer_(std::move(rtp_file_writer)) {
  RTC_DCHECK(task_queue);

  if (rtp_file_writer_.get()) {
    // Set us up as proxy receiver so that we can dump rtp packets.
    fake_network_.SetReceiver(this);
  }

  if (send_call_) {
    send_call_->SignalChannelNetworkState(MediaType::AUDIO, kNetworkUp);
    send_call_->SignalChannelNetworkState(MediaType::VIDEO, kNetworkUp);
  }
  SendPackets();
}

DirectTransport::~DirectTransport() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);
  // Constructor updates |next_scheduled_task_|, so it's guaranteed to
  // be initialized.
  task_queue_->CancelTask(next_scheduled_task_);
}

void DirectTransport::SetConfig(const FakeNetworkPipe::Config& config) {
  fake_network_.SetConfig(config);
}

void DirectTransport::StopSending() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);
  task_queue_->CancelTask(next_scheduled_task_);
}

void DirectTransport::SetReceiver(PacketReceiver* receiver) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);
  if (rtp_file_writer_.get()) {
    // Will be called from DeliverPacket() in this instance instead.
    receiver_ = receiver;
  } else {
    fake_network_.SetReceiver(receiver_);
  }
}

bool DirectTransport::SendRtp(const uint8_t* data,
                              size_t length,
                              const PacketOptions& options) {
  if (send_call_) {
    rtc::SentPacket sent_packet(options.packet_id,
                                clock_->TimeInMilliseconds());
    send_call_->OnSentPacket(sent_packet);
  }
  fake_network_.SendPacket(data, length);
  return true;
}

bool DirectTransport::SendRtcp(const uint8_t* data, size_t length) {
  fake_network_.SendPacket(data, length);
  return true;
}

int DirectTransport::GetAverageDelayMs() {
  return fake_network_.AverageDelay();
}

PacketReceiver::DeliveryStatus DirectTransport::DeliverPacket(
    MediaType media_type,
    const uint8_t* packet,
    size_t length,
    const PacketTime& packet_time) {
  if (rtp_file_writer_.get()) {
    RtpPacket rtp_packet;
    memcpy(rtp_packet.data, packet, length);
    rtp_packet.length = length;
    rtp_packet.original_length = length;
    rtp_packet.time_ms = rtc::TimeMillis() - start_ms_;
    rtp_file_writer_->WritePacket(&rtp_packet);
  }

  if (!receiver_)
    return PacketReceiver::DELIVERY_PACKET_ERROR;
  return receiver_->DeliverPacket(media_type, packet, length, packet_time);
}

void DirectTransport::SendPackets() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);

  fake_network_.Process();

  int64_t delay_ms = fake_network_.TimeUntilNextProcess();
  next_scheduled_task_ = task_queue_->PostDelayedTask([this]() {
    SendPackets();
  }, delay_ms);
}
}  // namespace test
}  // namespace webrtc
