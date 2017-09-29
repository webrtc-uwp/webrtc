/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "ARDBitrateAllocationStrategy.h"
#import "WebRTC/RTCBitrateAllocationStrategy.h"

#include "webrtc/rtc_base/bitrateallocationstrategy.h"

constexpr uint32_t kSufficientAudioBitrate = 16000;

@implementation ARDBitrateAllocationStrategy {
  rtc::AudioPriorityBitrateAllocationStrategy* audio_priority_strategy_;
  __weak RTCPeerConnection* _peerConnection;
}

- (id)init {
  if (self = [super init]) audio_priority_strategy_ = nullptr;
  _peerConnection = nil;
  return self;
}

- (void)setAudioPriorityStrategy:(RTCPeerConnection*)peerConnection
                    audioTrackId:(NSString*)audioTrackId {
  _peerConnection = peerConnection;
  audio_priority_strategy_ = new rtc::AudioPriorityBitrateAllocationStrategy(
      std::string(audioTrackId.UTF8String), kSufficientAudioBitrate);
  [peerConnection setBitrateAllocationStrategy:[[RTCBitrateAllocationStrategy alloc]
                                                   initWith:audio_priority_strategy_]];
}

- (void)dealloc {
  if (_peerConnection) {
    [_peerConnection setBitrateAllocationStrategy:nil];
  }
  if (audio_priority_strategy_) {
    delete audio_priority_strategy_;
    audio_priority_strategy_ = nullptr;
  }
}

@end
