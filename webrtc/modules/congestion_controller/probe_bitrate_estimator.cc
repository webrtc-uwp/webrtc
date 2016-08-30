/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/congestion_controller/probe_bitrate_estimator.h"

#include <algorithm>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"

namespace {
// Max number of saved clusters.
constexpr size_t kMaxNumSavedClusters = 5;

// The minumum number of probes we need for a valid cluster.
constexpr int kMinNumProbesValidCluster = 4;

// The maximum (receive rate)/(send rate) ratio for a valid estimate.
constexpr float kValidRatio = 2.0f;
}

namespace webrtc {

ProbingResult::ProbingResult() : bps(kNoEstimate), timestamp(0) {}

ProbingResult::ProbingResult(int bps, int64_t timestamp)
    : bps(bps), timestamp(timestamp) {}

bool ProbingResult::valid() const {
  return bps != kNoEstimate;
}

ProbeBitrateEstimator::ProbeBitrateEstimator() : last_valid_cluster_id_(0) {}

ProbingResult ProbeBitrateEstimator::PacketFeedback(
    const PacketInfo& packet_info) {
  // If this is not a probing packet or if this probing packet
  // belongs to an old cluster, do nothing.
  if (packet_info.probe_cluster_id == PacketInfo::kNotAProbe ||
      packet_info.probe_cluster_id < last_valid_cluster_id_) {
    return ProbingResult();
  }

  int payload_size_bits = packet_info.payload_size * 8;
  AggregatedCluster* cluster = &clusters_[packet_info.probe_cluster_id];

  // Clean up old clusters.
  while (clusters_.size() > kMaxNumSavedClusters)
    clusters_.erase(clusters_.begin());

  if (packet_info.send_time_ms < cluster->first_send_ms) {
    cluster->first_send_ms = packet_info.send_time_ms;
  }
  if (packet_info.send_time_ms > cluster->last_send_ms) {
    cluster->last_send_ms = packet_info.send_time_ms;
    cluster->size_last_send = payload_size_bits;
  }
  if (packet_info.arrival_time_ms < cluster->first_receive_ms) {
    cluster->first_receive_ms = packet_info.arrival_time_ms;
    cluster->size_first_receive = payload_size_bits;
  }
  if (packet_info.arrival_time_ms > cluster->last_receive_ms) {
    cluster->last_receive_ms = packet_info.arrival_time_ms;
  }
  cluster->size_total += payload_size_bits;
  cluster->num_probes += 1;

  if (cluster->num_probes < kMinNumProbesValidCluster)
    return ProbingResult();

  float send_interval_ms = cluster->last_send_ms - cluster->first_send_ms;
  float receive_interval_ms =
      cluster->last_receive_ms - cluster->first_receive_ms;

  if (send_interval_ms == 0 || receive_interval_ms == 0) {
    LOG(LS_INFO) << "Probing unsuccessful, invalid send/receive interval"
                 << " [cluster id: " << packet_info.probe_cluster_id
                 << "] [send interval: " << send_interval_ms << " ms]"
                 << " [receive interval: " << receive_interval_ms << " ms]";

    return ProbingResult();
  }
  // Since the |send_interval_ms| does not include the time it takes to actually
  // send the last packet the size of the last sent packet should not be
  // included when calculating the send bitrate.
  RTC_DCHECK_GT(cluster->size_total, cluster->size_last_send);
  float send_size = cluster->size_total - cluster->size_last_send;
  float send_bps = send_size / send_interval_ms * 1000;

  // Since the |receive_interval_ms| does not include the time it takes to
  // actually receive the first packet the size of the first received packet
  // should not be included when calculating the receive bitrate.
  RTC_DCHECK_GT(cluster->size_total, cluster->size_first_receive);
  float receive_size = cluster->size_total - cluster->size_first_receive;
  float receive_bps = receive_size / receive_interval_ms * 1000;

  float ratio = receive_bps / send_bps;
  if (ratio > kValidRatio) {
    LOG(LS_INFO) << "Probing unsuccessful, receive/send ratio too high"
                 << " [cluster id: " << packet_info.probe_cluster_id
                 << "] [send: " << send_size << " bytes / " << send_interval_ms
                 << " ms = " << send_bps / 1000 << " kb/s]"
                 << " [receive: " << receive_size << " bytes / "
                 << receive_interval_ms << " ms = " << receive_bps / 1000
                 << " kb/s]"
                 << " [ratio: " << receive_bps / 1000 << " / "
                 << send_bps / 1000 << " = " << ratio << " > kValidRatio ("
                 << kValidRatio << ")]";

    return ProbingResult();
  }
  // We have a valid estimate.
  int result_bps = std::min(send_bps, receive_bps);
  last_valid_cluster_id_ = packet_info.probe_cluster_id;
  LOG(LS_INFO) << "Probing successful"
               << " [cluster id: " << packet_info.probe_cluster_id
               << "] [send: " << send_size << " bytes / " << send_interval_ms
               << " ms = " << send_bps / 1000 << " kb/s]"
               << " [receive: " << receive_size << " bytes / "
               << receive_interval_ms << " ms = " << receive_bps / 1000
               << " kb/s]";

  return ProbingResult(result_bps, packet_info.arrival_time_ms);
}
}  // namespace webrtc
