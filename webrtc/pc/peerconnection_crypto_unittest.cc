/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/p2p/base/fakeportallocator.h"
#include "webrtc/pc/mediasession.h"
#include "webrtc/pc/peerconnectionunittestfixture.h"
#include "webrtc/pc/test/fakertccertificategenerator.h"
#include "webrtc/rtc_base/ptr_util.h"

namespace {

using RTCConfiguration = webrtc::PeerConnectionInterface::RTCConfiguration;
using webrtc::PeerConnectionInterface;
using webrtc::PeerConnectionFactoryInterface;
using webrtc::SessionDescriptionInterface;

class PeerConnectionWrapperForCryptoUnitTest
    : public webrtc::PeerConnectionWrapper {
 public:
  PeerConnectionWrapperForCryptoUnitTest(
      rtc::scoped_refptr<PeerConnectionFactoryInterface> pc_factory)
      : PeerConnectionWrapper(pc_factory) {}

  bool Initialize(const RTCConfiguration& config) {
    auto fake_port_allocator = rtc::MakeUnique<cricket::FakePortAllocator>(
        rtc::Thread::Current(), nullptr);

    std::unique_ptr<FakeRTCCertificateGenerator> cert_generator;
    if (config.enable_dtls_srtp.value_or(false) &&
        config.certificates.empty()) {
      cert_generator.reset(new FakeRTCCertificateGenerator());
    }
    fake_certificate_generator_ = cert_generator.get();

    return InitializePeerConnection(config, std::move(fake_port_allocator),
                                    std::move(cert_generator));
  }

  FakeRTCCertificateGenerator* fake_certificate_generator_ = nullptr;
};

class PeerConnectionCryptoUnitTest
    : public webrtc::PeerConnectionUnitTestFixture {
 protected:
  typedef std::unique_ptr<PeerConnectionWrapperForCryptoUnitTest> WrapperPtr;

  WrapperPtr CreatePeerConnection(const RTCConfiguration& config) {
    auto wrapper =
        rtc::MakeUnique<PeerConnectionWrapperForCryptoUnitTest>(pc_factory_);
    if (!wrapper->Initialize(config)) {
      return nullptr;
    }
    return wrapper;
  }

  WrapperPtr CreatePeerConnectionWithAudioVideo(
      const RTCConfiguration& config) {
    auto wrapper = CreatePeerConnection(config);
    if (!wrapper) {
      return nullptr;
    }
    wrapper->AddAudioVideoStream("s", "a", "v");
    return wrapper;
  }

  typedef std::function<bool(const cricket::ContentInfo*,
                             const cricket::TransportInfo*)>
      SdpContentPredicate;

  bool SdpContentsAll(SdpContentPredicate pred,
                      const cricket::SessionDescription* desc) {
    for (const auto& content : desc->contents()) {
      const auto* transport_info = desc->GetTransportInfoByName(content.name);
      if (!pred(&content, transport_info)) {
        return false;
      }
    }
    return true;
  }

  bool SdpContentsNone(SdpContentPredicate pred,
                       const cricket::SessionDescription* desc) {
    return SdpContentsAll(std::not2(pred), desc);
  }

  SdpContentPredicate HaveDtlsFingerprint() {
    return [](const cricket::ContentInfo* content,
              const cricket::TransportInfo* transport) {
      return transport->description.identity_fingerprint != nullptr;
    };
  }

  SdpContentPredicate HaveSdesCryptos() {
    return [](const cricket::ContentInfo* content,
              const cricket::TransportInfo* transport) {
      const auto* media_desc =
          static_cast<const cricket::MediaContentDescription*>(
              content->description);
      return !media_desc->cryptos().empty();
    };
  }

  SdpContentPredicate HaveProtocol(const std::string& protocol) {
    return [protocol](const cricket::ContentInfo* content,
                      const cricket::TransportInfo* transport) {
      const auto* media_desc =
          static_cast<const cricket::MediaContentDescription*>(
              content->description);
      return media_desc->protocol() == protocol;
    };
  }

  SdpContentPredicate HaveSdesGcmCryptos(size_t num_crypto_suites) {
    return [num_crypto_suites](const cricket::ContentInfo* content,
                               const cricket::TransportInfo* transport) {
      const auto* media_desc =
          static_cast<const cricket::MediaContentDescription*>(
              content->description);
      if (media_desc->cryptos().size() != num_crypto_suites) {
        return false;
      }
      const cricket::CryptoParams first_params = media_desc->cryptos()[0];
      return first_params.key_params.size() == 67U &&
             first_params.cipher_suite == "AEAD_AES_256_GCM";
    };
  }

  typedef std::function<void(cricket::ContentInfo*, cricket::TransportInfo*)>
      SdpContentMutator;

  void SdpContentsForEach(SdpContentMutator fn,
                          cricket::SessionDescription* desc) {
    for (auto& content : desc->contents()) {
      auto* transport_info = desc->GetTransportInfoByName(content.name);
      fn(&content, transport_info);
    }
  }

  std::unique_ptr<SessionDescriptionInterface> SdpMutateContents(
      SdpContentMutator fn,
      std::unique_ptr<SessionDescriptionInterface> sdesc) {
    SdpContentsForEach(fn, sdesc->description());
    return sdesc;
  }

  SdpContentMutator RemoveSdesCryptos() {
    return [](cricket::ContentInfo* content,
              cricket::TransportInfo* transport) {
      auto* media_desc =
          static_cast<cricket::MediaContentDescription*>(content->description);
      media_desc->set_cryptos({});
    };
  }

  SdpContentMutator RemoveDtlsFingerprint() {
    return
        [](cricket::ContentInfo* content, cricket::TransportInfo* transport) {
          transport->description.identity_fingerprint.reset();
        };
  }
};

// When DTLS is enabled, the SDP offer/answer should have a DTLS fingerprint and
// no SDES cryptos.
TEST_F(PeerConnectionCryptoUnitTest, CorrectCryptoInOfferWhenDtlsEnabled) {
  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(true);
  auto caller = CreatePeerConnectionWithAudioVideo(config);

  auto offer = caller->CreateOffer();
  ASSERT_TRUE(offer);

  ASSERT_FALSE(offer->description()->contents().empty());
  EXPECT_TRUE(SdpContentsAll(HaveDtlsFingerprint(), offer->description()));
  EXPECT_TRUE(SdpContentsNone(HaveSdesCryptos(), offer->description()));
  EXPECT_TRUE(SdpContentsAll(HaveProtocol(cricket::kMediaProtocolDtlsSavpf),
                             offer->description()));
}
TEST_F(PeerConnectionCryptoUnitTest, CorrectCryptoInAnswerWhenDtlsEnabled) {
  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(true);
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  auto callee = CreatePeerConnectionWithAudioVideo(config);

  callee->SetRemoteDescription(caller->CreateOffer());
  auto answer = callee->CreateAnswer();
  ASSERT_TRUE(answer);

  ASSERT_FALSE(answer->description()->contents().empty());
  EXPECT_TRUE(SdpContentsAll(HaveDtlsFingerprint(), answer->description()));
  EXPECT_TRUE(SdpContentsNone(HaveSdesCryptos(), answer->description()));
  EXPECT_TRUE(SdpContentsAll(HaveProtocol(cricket::kMediaProtocolDtlsSavpf),
                             answer->description()));
}

// When DTLS is disabled, the SDP offer/answer should include SDES cryptos and
// should not have a DTLS fingerprint.
TEST_F(PeerConnectionCryptoUnitTest, CorrectCryptoInOfferWhenDtlsDisabled) {
  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(false);
  auto caller = CreatePeerConnectionWithAudioVideo(config);

  auto offer = caller->CreateOffer();
  ASSERT_TRUE(offer);

  ASSERT_FALSE(offer->description()->contents().empty());
  EXPECT_TRUE(SdpContentsAll(HaveSdesCryptos(), offer->description()));
  EXPECT_TRUE(SdpContentsNone(HaveDtlsFingerprint(), offer->description()));
  EXPECT_TRUE(SdpContentsAll(HaveProtocol(cricket::kMediaProtocolSavpf),
                             offer->description()));
}
TEST_F(PeerConnectionCryptoUnitTest, CorrectCryptoInAnswerWhenDtlsDisabled) {
  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(false);
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  auto callee = CreatePeerConnectionWithAudioVideo(config);

  callee->SetRemoteDescription(caller->CreateOffer());
  auto answer = callee->CreateAnswer();
  ASSERT_TRUE(answer);

  ASSERT_FALSE(answer->description()->contents().empty());
  EXPECT_TRUE(SdpContentsAll(HaveSdesCryptos(), answer->description()));
  EXPECT_TRUE(SdpContentsNone(HaveDtlsFingerprint(), answer->description()));
  EXPECT_TRUE(SdpContentsAll(HaveProtocol(cricket::kMediaProtocolSavpf),
                             answer->description()));
}

// When encryption is disabled, the SDP offer/answer should have neither a DTLS
// fingerprint nor any SDES crypto options.
TEST_F(PeerConnectionCryptoUnitTest,
       CorrectCryptoInOfferWhenEncryptionDisabled) {
  PeerConnectionFactoryInterface::Options options;
  options.disable_encryption = true;
  pc_factory_->SetOptions(options);

  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(false);
  auto caller = CreatePeerConnectionWithAudioVideo(config);

  auto offer = caller->CreateOffer();
  ASSERT_TRUE(offer);

  ASSERT_FALSE(offer->description()->contents().empty());
  EXPECT_TRUE(SdpContentsNone(HaveSdesCryptos(), offer->description()));
  EXPECT_TRUE(SdpContentsNone(HaveDtlsFingerprint(), offer->description()));
  EXPECT_TRUE(SdpContentsAll(HaveProtocol(cricket::kMediaProtocolAvpf),
                             offer->description()));
}
TEST_F(PeerConnectionCryptoUnitTest,
       CorrectCryptoInAnswerWhenEncryptionDisabled) {
  PeerConnectionFactoryInterface::Options options;
  options.disable_encryption = true;
  pc_factory_->SetOptions(options);

  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(false);
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  auto callee = CreatePeerConnectionWithAudioVideo(config);

  callee->SetRemoteDescription(caller->CreateOffer());
  auto answer = callee->CreateAnswer();
  ASSERT_TRUE(answer);

  ASSERT_FALSE(answer->description()->contents().empty());
  EXPECT_TRUE(SdpContentsNone(HaveSdesCryptos(), answer->description()));
  EXPECT_TRUE(SdpContentsNone(HaveDtlsFingerprint(), answer->description()));
  EXPECT_TRUE(SdpContentsAll(HaveProtocol(cricket::kMediaProtocolAvpf),
                             answer->description()));
}

// When DTLS is disabled and GCM cipher suites are enabled, the SDP offer/answer
// should have the correct ciphers in the SDES crypto options.
// With GCM cipher suites enabled, there will be 3 cryptos in the offer and 1
// in the answer.
TEST_F(PeerConnectionCryptoUnitTest, CorrectCryptoInOfferWhenSdesAndGcm) {
  PeerConnectionFactoryInterface::Options options;
  options.crypto_options.enable_gcm_crypto_suites = true;
  pc_factory_->SetOptions(options);

  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(false);
  auto caller = CreatePeerConnectionWithAudioVideo(config);

  auto offer = caller->CreateOffer();
  ASSERT_TRUE(offer);

  ASSERT_FALSE(offer->description()->contents().empty());
  EXPECT_TRUE(SdpContentsAll(HaveSdesGcmCryptos(3), offer->description()));
}
TEST_F(PeerConnectionCryptoUnitTest, CorrectCryptoInAnswerWhenSdesAndGcm) {
  PeerConnectionFactoryInterface::Options options;
  options.crypto_options.enable_gcm_crypto_suites = true;
  pc_factory_->SetOptions(options);

  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(false);
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  auto callee = CreatePeerConnectionWithAudioVideo(config);

  callee->SetRemoteDescription(caller->CreateOffer());
  auto answer = callee->CreateAnswer();
  ASSERT_TRUE(answer);

  ASSERT_FALSE(answer->description()->contents().empty());
  EXPECT_TRUE(SdpContentsAll(HaveSdesGcmCryptos(1), answer->description()));
}

TEST_F(PeerConnectionCryptoUnitTest, CanSetSdesGcmRemoteOfferAndLocalAnswer) {
  PeerConnectionFactoryInterface::Options options;
  options.crypto_options.enable_gcm_crypto_suites = true;
  pc_factory_->SetOptions(options);

  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(false);
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  auto callee = CreatePeerConnectionWithAudioVideo(config);

  auto offer = caller->CreateOffer();
  ASSERT_TRUE(offer);
  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));

  auto answer = callee->CreateAnswer();
  ASSERT_TRUE(answer);
  ASSERT_TRUE(callee->SetLocalDescription(std::move(answer)));
}

// The following group tests that two PeerConnections can successfully exchange
// an offer/answer when DTLS is off and that they will refuse any offer/answer
// applied locally/remotely if it does not include SDES cryptos.
TEST_F(PeerConnectionCryptoUnitTest, ExchangeOfferAnswerWhenSdesOn) {
  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(false);
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  auto callee = CreatePeerConnectionWithAudioVideo(config);

  auto offer = caller->CreateOfferAndSetAsLocal();
  ASSERT_TRUE(offer);
  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));

  auto answer = callee->CreateAnswerAndSetAsLocal();
  ASSERT_TRUE(answer);
  ASSERT_TRUE(caller->SetRemoteDescription(std::move(answer)));
}
TEST_F(PeerConnectionCryptoUnitTest,
       FailToSetLocalOfferWithNoCryptosWhenSdesOn) {
  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(false);
  auto caller = CreatePeerConnectionWithAudioVideo(config);

  auto offer = caller->CreateOffer();
  SdpContentsForEach(RemoveSdesCryptos(), offer->description());

  EXPECT_FALSE(caller->SetLocalDescription(std::move(offer)));
}
TEST_F(PeerConnectionCryptoUnitTest,
       FailToSetRemoteOfferWithNoCryptosWhenSdesOn) {
  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(false);
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  auto callee = CreatePeerConnectionWithAudioVideo(config);

  auto offer = caller->CreateOffer();
  SdpContentsForEach(RemoveSdesCryptos(), offer->description());

  EXPECT_FALSE(callee->SetRemoteDescription(std::move(offer)));
}
TEST_F(PeerConnectionCryptoUnitTest,
       FailToSetLocalAnswerWithNoCryptosWhenSdesOn) {
  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(false);
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  auto callee = CreatePeerConnectionWithAudioVideo(config);

  callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal());
  auto answer = callee->CreateAnswer();
  SdpContentsForEach(RemoveSdesCryptos(), answer->description());
}
TEST_F(PeerConnectionCryptoUnitTest,
       FailToSetRemoteAnswerWithNoCryptosWhenSdesOn) {
  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(false);
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  auto callee = CreatePeerConnectionWithAudioVideo(config);

  callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal());
  auto answer = callee->CreateAnswerAndSetAsLocal();
  SdpContentsForEach(RemoveSdesCryptos(), answer->description());

  EXPECT_FALSE(caller->SetRemoteDescription(std::move(answer)));
}

// The following group tests that two PeerConnections can successfully exchange
// an offer/answer when DTLS is on and that they will refuse any offer/answer
// applied locally/remotely if it does not include a DTLS fingerprint.
TEST_F(PeerConnectionCryptoUnitTest, ExchangeOfferAnswerWhenDtlsOn) {
  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(true);
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  auto callee = CreatePeerConnectionWithAudioVideo(config);

  auto offer = caller->CreateOfferAndSetAsLocal();
  ASSERT_TRUE(offer);
  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));

  auto answer = callee->CreateAnswerAndSetAsLocal();
  ASSERT_TRUE(answer);
  ASSERT_TRUE(caller->SetRemoteDescription(std::move(answer)));
}
TEST_F(PeerConnectionCryptoUnitTest,
       FailToSetLocalOfferWithNoFingerprintWhenDtlsOn) {
  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(true);
  auto caller = CreatePeerConnectionWithAudioVideo(config);

  auto offer = caller->CreateOffer();
  SdpContentsForEach(RemoveDtlsFingerprint(), offer->description());

  EXPECT_FALSE(caller->SetLocalDescription(std::move(offer)));
}
TEST_F(PeerConnectionCryptoUnitTest,
       FailToSetRemoteOfferWithNoFingerprintWhenDtlsOn) {
  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(true);
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  auto callee = CreatePeerConnectionWithAudioVideo(config);

  auto offer = caller->CreateOffer();
  SdpContentsForEach(RemoveDtlsFingerprint(), offer->description());

  EXPECT_FALSE(callee->SetRemoteDescription(std::move(offer)));
}
TEST_F(PeerConnectionCryptoUnitTest,
       FailToSetLocalAnswerWithNoFingerprintWhenDtlsOn) {
  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(true);
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  auto callee = CreatePeerConnectionWithAudioVideo(config);

  callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal());
  auto answer = callee->CreateAnswer();
  SdpContentsForEach(RemoveDtlsFingerprint(), answer->description());
}
TEST_F(PeerConnectionCryptoUnitTest,
       FailToSetRemoteAnswerWithNoFingerprintWhenDtlsOn) {
  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(true);
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  auto callee = CreatePeerConnectionWithAudioVideo(config);

  callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal());
  auto answer = callee->CreateAnswerAndSetAsLocal();
  SdpContentsForEach(RemoveDtlsFingerprint(), answer->description());

  EXPECT_FALSE(caller->SetRemoteDescription(std::move(answer)));
}

// Test that an offer/answer can be exchanged when encryption is disabled.
TEST_F(PeerConnectionCryptoUnitTest, ExchangeOfferAnswerWhenNoEncryption) {
  PeerConnectionFactoryInterface::Options options;
  options.disable_encryption = true;
  pc_factory_->SetOptions(options);

  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(false);
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  auto callee = CreatePeerConnectionWithAudioVideo(config);

  auto offer = caller->CreateOfferAndSetAsLocal();
  ASSERT_TRUE(offer);
  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));

  auto answer = callee->CreateAnswerAndSetAsLocal();
  ASSERT_TRUE(answer);
  ASSERT_TRUE(caller->SetRemoteDescription(std::move(answer)));
}

}  // namespace
