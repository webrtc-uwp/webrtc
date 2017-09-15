/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/peerconnectionunittestfixture.h"

#include <memory>
#include <string>
#include <utility>

#include "api/jsepsessiondescription.h"
#include "media/base/fakevideocapturer.h"
#ifdef WEBRTC_ANDROID
#include "pc/test/androidtestinitializer.h"
#endif
#include "pc/test/fakeaudiocapturemodule.h"
#include "rtc_base/gunit.h"
#include "rtc_base/ptr_util.h"

namespace webrtc {

static const uint32_t kWaitTimeout = 10000U;

PeerConnectionWrapper::PeerConnectionWrapper(
    rtc::scoped_refptr<PeerConnectionFactoryInterface> pc_factory)
    : pc_factory_(pc_factory) {
  RTC_DCHECK(pc_factory_);
}

PeerConnectionWrapper::~PeerConnectionWrapper() = default;

PeerConnectionFactoryInterface* PeerConnectionWrapper::pc_factory() {
  return pc_factory_.get();
}

PeerConnectionInterface* PeerConnectionWrapper::pc() {
  RTC_DCHECK(pc_);
  return pc_.get();
}

MockPeerConnectionObserver* PeerConnectionWrapper::observer() {
  return &observer_;
}

bool PeerConnectionWrapper::InitializePeerConnection(
    const PeerConnectionInterface::RTCConfiguration& config,
    std::unique_ptr<cricket::PortAllocator> port_allocator,
    std::unique_ptr<rtc::RTCCertificateGeneratorInterface> cert_generator) {
  pc_ = pc_factory()->CreatePeerConnection(
      config, std::move(port_allocator), std::move(cert_generator), observer());
  if (!pc_ || pc_->signaling_state() != PeerConnectionInterface::kStable) {
    return false;
  }
  observer()->SetPeerConnectionInterface(pc_.get());
  return true;
}

std::unique_ptr<SessionDescriptionInterface>
PeerConnectionWrapper::CreateOffer() {
  return CreateOffer(PeerConnectionInterface::RTCOfferAnswerOptions());
}

std::unique_ptr<SessionDescriptionInterface> PeerConnectionWrapper::CreateOffer(
    const PeerConnectionInterface::RTCOfferAnswerOptions& options) {
  return CreateSdp([this, options](CreateSessionDescriptionObserver* observer) {
    pc()->CreateOffer(observer, options);
  });
}

std::unique_ptr<SessionDescriptionInterface>
PeerConnectionWrapper::CreateOfferAndSetAsLocal() {
  auto offer = CreateOffer();
  if (!offer) {
    return nullptr;
  }
  EXPECT_TRUE(SetLocalDescription(CloneSessionDescription(offer.get())));
  return offer;
}

std::unique_ptr<SessionDescriptionInterface>
PeerConnectionWrapper::CreateAnswer() {
  return CreateAnswer(PeerConnectionInterface::RTCOfferAnswerOptions());
}

std::unique_ptr<SessionDescriptionInterface>
PeerConnectionWrapper::CreateAnswer(
    const PeerConnectionInterface::RTCOfferAnswerOptions& options) {
  return CreateSdp([this, options](CreateSessionDescriptionObserver* observer) {
    pc()->CreateAnswer(observer, options);
  });
}

std::unique_ptr<SessionDescriptionInterface>
PeerConnectionWrapper::CreateAnswerAndSetAsLocal() {
  auto answer = CreateAnswer();
  if (!answer) {
    return nullptr;
  }
  EXPECT_TRUE(SetLocalDescription(CloneSessionDescription(answer.get())));
  return answer;
}

std::unique_ptr<SessionDescriptionInterface> PeerConnectionWrapper::CreateSdp(
    std::function<void(CreateSessionDescriptionObserver*)> fn) {
  rtc::scoped_refptr<MockCreateSessionDescriptionObserver> observer(
      new rtc::RefCountedObject<MockCreateSessionDescriptionObserver>());
  fn(observer);
  EXPECT_EQ_WAIT(true, observer->called(), kWaitTimeout);
  return observer->MoveDescription();
}

bool PeerConnectionWrapper::SetLocalDescription(
    std::unique_ptr<SessionDescriptionInterface> desc) {
  return SetSdp([this, &desc](SetSessionDescriptionObserver* observer) {
    pc()->SetLocalDescription(observer, desc.release());
  });
}

bool PeerConnectionWrapper::SetRemoteDescription(
    std::unique_ptr<SessionDescriptionInterface> desc) {
  return SetSdp([this, &desc](SetSessionDescriptionObserver* observer) {
    pc()->SetRemoteDescription(observer, desc.release());
  });
}

bool PeerConnectionWrapper::SetSdp(
    std::function<void(SetSessionDescriptionObserver*)> fn) {
  rtc::scoped_refptr<MockSetSessionDescriptionObserver> observer(
      new rtc::RefCountedObject<MockSetSessionDescriptionObserver>());
  fn(observer);
  if (pc()->signaling_state() != PeerConnectionInterface::kClosed) {
    EXPECT_EQ_WAIT(true, observer->called(), kWaitTimeout);
  }
  return observer->result();
}

std::unique_ptr<SessionDescriptionInterface>
PeerConnectionWrapper::CloneSessionDescription(
    const SessionDescriptionInterface* desc) {
  std::unique_ptr<JsepSessionDescription> clone(
      new JsepSessionDescription(desc->type()));
  RTC_DCHECK(clone->Initialize(desc->description()->Copy(), desc->session_id(),
                               desc->session_version()));
  return clone;
}

void PeerConnectionWrapper::AddAudioStream(const std::string& stream_label,
                                           const std::string& track_label) {
  auto stream = pc_factory()->CreateLocalMediaStream(stream_label);
  auto audio_track = pc_factory()->CreateAudioTrack(track_label, nullptr);
  EXPECT_TRUE(pc()->AddTrack(audio_track, {stream}));
  EXPECT_TRUE_WAIT(observer()->renegotiation_needed_, kWaitTimeout);
  observer()->renegotiation_needed_ = false;
}

void PeerConnectionWrapper::AddVideoStream(const std::string& stream_label,
                                           const std::string& track_label) {
  auto stream = pc_factory()->CreateLocalMediaStream(stream_label);
  auto video_source = pc_factory()->CreateVideoSource(
      rtc::MakeUnique<cricket::FakeVideoCapturer>());
  auto video_track = pc_factory()->CreateVideoTrack(track_label, video_source);
  EXPECT_TRUE(pc()->AddTrack(video_track, {stream}));
  EXPECT_TRUE_WAIT(observer()->renegotiation_needed_, kWaitTimeout);
  observer()->renegotiation_needed_ = false;
}

void PeerConnectionWrapper::AddAudioVideoStream(
    const std::string& stream_label,
    const std::string& audio_track_label,
    const std::string& video_track_label) {
  auto stream = pc_factory()->CreateLocalMediaStream(stream_label);
  auto audio_track = pc_factory()->CreateAudioTrack(audio_track_label, nullptr);
  EXPECT_TRUE(pc()->AddTrack(audio_track, {stream}));
  auto video_source = pc_factory()->CreateVideoSource(
      rtc::MakeUnique<cricket::FakeVideoCapturer>());
  auto video_track =
      pc_factory()->CreateVideoTrack(video_track_label, video_source);
  EXPECT_TRUE(pc()->AddTrack(video_track, {stream}));
  EXPECT_TRUE_WAIT(observer()->renegotiation_needed_, kWaitTimeout);
  observer()->renegotiation_needed_ = false;
}

PeerConnectionUnitTestFixture::PeerConnectionUnitTestFixture()
    : vss_(new rtc::VirtualSocketServer()), main_(vss_.get()) {
#ifdef WEBRTC_ANDROID
  webrtc::InitializeAndroidObjects();
#endif
  pc_factory_ = CreatePeerConnectionFactory();
  RTC_DCHECK(pc_factory_);
}

rtc::scoped_refptr<PeerConnectionFactoryInterface>
PeerConnectionUnitTestFixture::CreatePeerConnectionFactory() {
  return webrtc::CreatePeerConnectionFactory(
      rtc::Thread::Current(), rtc::Thread::Current(), rtc::Thread::Current(),
      FakeAudioCaptureModule::Create(), nullptr, nullptr);
}

}  // namespace webrtc
