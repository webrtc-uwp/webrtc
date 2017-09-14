/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_PC_PEERCONNECTIONUNITTESTFIXTURE_H_
#define WEBRTC_PC_PEERCONNECTIONUNITTESTFIXTURE_H_

#include <functional>
#include <memory>
#include <string>

#include "webrtc/api/peerconnectioninterface.h"
#include "webrtc/pc/test/mockpeerconnectionobservers.h"
#include "webrtc/rtc_base/gunit.h"
#include "webrtc/rtc_base/thread.h"
#include "webrtc/rtc_base/virtualsocketserver.h"

namespace webrtc {

// Class that wraps a PeerConnection so that it is easier to use in unit tests.
// Namely, gives a synchronous API for the event-callback-based API of
// PeerConnection.
//
// This is intended to be subclassed if additional information needs to be
// stored with the PeerConnection (e.g., fake PeerConnection parameters so that
// tests can be written against those interactions). The base
// PeerConnectionWrapper should only have helper methods that are broadly
// useful. More specific helper methods should be created in the test-specific
// subclass.
//
// The wrapper should be constructed by a factory method on the
// PeerConnectionUnitTestFixture. That factory method must call
// InitializePeerConnection before returning the the wrapper to the unit test
// code.
class PeerConnectionWrapper {
 public:
  PeerConnectionWrapper(
      rtc::scoped_refptr<PeerConnectionFactoryInterface> pc_factory);
  virtual ~PeerConnectionWrapper();

  PeerConnectionFactoryInterface* pc_factory();
  PeerConnectionInterface* pc();
  MockPeerConnectionObserver* observer();

  // Calls the underlying PeerConnection's CreateOffer method and returns the
  // resulting SessionDescription once it is available. If the method call
  // failed, null is returned.
  std::unique_ptr<SessionDescriptionInterface> CreateOffer(
      const PeerConnectionInterface::RTCOfferAnswerOptions& options);
  // Calls CreateOffer with default options.
  std::unique_ptr<SessionDescriptionInterface> CreateOffer();
  // Calls CreateOffer and sets a copy of the offer as the local description.
  std::unique_ptr<SessionDescriptionInterface> CreateOfferAndSetAsLocal();

  // Calls the underlying PeerConnection's CreateAnswer method and returns the
  // resulting SessionDescription once it is available. If the method call
  // failed, null is returned.
  std::unique_ptr<SessionDescriptionInterface> CreateAnswer(
      const PeerConnectionInterface::RTCOfferAnswerOptions& options);
  // Calls CreateAnswer with the default options.
  std::unique_ptr<SessionDescriptionInterface> CreateAnswer();
  // Calls CreateAnswer and sets a copy of the offer as the local description.
  std::unique_ptr<SessionDescriptionInterface> CreateAnswerAndSetAsLocal();

  // Calls the underlying PeerConnection's SetLocalDescription method with the
  // given session description and waits for the success/failure response.
  // Returns true if the description was successfully set.
  bool SetLocalDescription(std::unique_ptr<SessionDescriptionInterface> desc);
  // Calls the underlying PeerConnection's SetRemoteDescription method with the
  // given session description and waits for the success/failure response.
  // Returns true if the description was successfully set.
  bool SetRemoteDescription(std::unique_ptr<SessionDescriptionInterface> desc);

  // Adds a new stream with one audio track to the underlying PeerConnection.
  void AddAudioStream(const std::string& stream_label,
                      const std::string& track_label);
  // Adds a new stream with one video track to the underlying PeerConnection.
  void AddVideoStream(const std::string& stream_label,
                      const std::string& track_label);
  // Adds a new stream with one audio and one video track to the underlying
  // PeerConnection.
  void AddAudioVideoStream(const std::string& stream_label,
                           const std::string& audio_track_label,
                           const std::string& video_track_label);

 protected:
  // Creates the underlying PeerConnection with the given parameters. Registers
  // the MockPeerConnectionObserver with the new PeerConnection to receive
  // events. Returns true if the PeerConnection was created successfully.
  bool InitializePeerConnection(
      const PeerConnectionInterface::RTCConfiguration& config,
      std::unique_ptr<cricket::PortAllocator> port_allocator,
      std::unique_ptr<rtc::RTCCertificateGeneratorInterface> cert_generator);

 private:
  std::unique_ptr<SessionDescriptionInterface> CreateSdp(
      std::function<void(CreateSessionDescriptionObserver*)> fn);
  bool SetSdp(std::function<void(SetSessionDescriptionObserver*)> fn);
  std::unique_ptr<SessionDescriptionInterface> CloneSessionDescription(
      const SessionDescriptionInterface* desc);

  rtc::scoped_refptr<PeerConnectionInterface> pc_;
  rtc::scoped_refptr<PeerConnectionFactoryInterface> pc_factory_;
  MockPeerConnectionObserver observer_;
};

// Base test fixture for PeerConnection unit tests.
class PeerConnectionUnitTestFixture : public testing::Test {
 protected:
  PeerConnectionUnitTestFixture();

  // Creates the PeerConnectionFactory for this unit test fixture.
  // By default, this creates a PeerConnectionFactory with:
  // - A virtual socket server installed on the main thread
  // - A fake AudioCaptureModule
  // - Default video encoder/decoder factories
  // Intended to be overridden if the default configuration is not suitable for
  // the unit test.
  virtual rtc::scoped_refptr<PeerConnectionFactoryInterface>
  CreatePeerConnectionFactory();

  std::unique_ptr<rtc::VirtualSocketServer> vss_;
  rtc::AutoSocketServerThread main_;
  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> pc_factory_;
};

}  // namespace webrtc

#endif  // WEBRTC_PC_PEERCONNECTIONUNITTESTFIXTURE_H_
