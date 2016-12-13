// https://stackoverflow.com/questions/34013930/error-c4592-symbol-will-be-dynamically-initialized-vs2015-1-static-const-std
#pragma warning( disable : 4592)

// Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "webrtc/build/WinRT_gyp/Api/Marshalling.h"
#include <map>

#include "webrtc/p2p/base/candidate.h"
#include "webrtc/api/webrtcsdp.h"
#include "webrtc/build/WinRT_gyp/Api/RTCStatsReport.h"

using Org::WebRtc::RTCBundlePolicy;
using Org::WebRtc::RTCIceTransportPolicy;
using Org::WebRtc::RTCSignalingState;
using Org::WebRtc::RTCDataChannelState;
using Org::WebRtc::RTCIceGatheringState;
using Org::WebRtc::RTCIceConnectionState;
using Org::WebRtc::RTCIceServer;
using Org::WebRtc::RTCConfiguration;
using Org::WebRtc::RTCStatsValueName;
using Org::WebRtc::RTCStatsType;

namespace Org {
	namespace WebRtc {
		namespace Internal {

#define DEFINE_MARSHALLED_ENUM(name, winrtType, nativeType)\
  typedef winrtType name##_winrtType;\
  typedef nativeType name##_nativeType;\
  std::map<winrtType, nativeType> name##_enumMap = {
#define DEFINE_MARSHALLED_ENUM_END(name)\
  };\
  void FromCx(name##_winrtType inObj, name##_nativeType* outObj) {\
    (*outObj) = name##_enumMap[inObj];\
  }\
  void ToCx(name##_nativeType const& inObj, name##_winrtType* outObj) {\
    for (auto& kv : name##_enumMap) {\
      if (kv.second == inObj) {\
        (*outObj) = kv.first;\
        return;\
      }\
    }\
    throw "Marshalling failed";\
  }\

			DEFINE_MARSHALLED_ENUM(BundlePolicy,
				RTCBundlePolicy, webrtc::PeerConnectionInterface::BundlePolicy)
			{
				RTCBundlePolicy::Balanced,
					webrtc::PeerConnectionInterface::kBundlePolicyBalanced
			},
			{ RTCBundlePolicy::MaxBundle,
				webrtc::PeerConnectionInterface::kBundlePolicyMaxBundle },
				{ RTCBundlePolicy::MaxCompat,
					webrtc::PeerConnectionInterface::kBundlePolicyMaxCompat },
					DEFINE_MARSHALLED_ENUM_END(BundlePolicy)

					DEFINE_MARSHALLED_ENUM(IceTransportPolicy,
						RTCIceTransportPolicy, webrtc::PeerConnectionInterface::IceTransportsType)
				{
					RTCIceTransportPolicy::None,
						webrtc::PeerConnectionInterface::kNone
				},
				{ RTCIceTransportPolicy::Relay,
					webrtc::PeerConnectionInterface::kRelay },
					{ RTCIceTransportPolicy::NoHost,
						webrtc::PeerConnectionInterface::kNoHost },
						{ RTCIceTransportPolicy::All,
							webrtc::PeerConnectionInterface::kAll },
						DEFINE_MARSHALLED_ENUM_END(IceTransportPolicy)

						DEFINE_MARSHALLED_ENUM(SignalingState,
							RTCSignalingState, webrtc::PeerConnectionInterface::SignalingState)
					{
						RTCSignalingState::Stable,
							webrtc::PeerConnectionInterface::kStable
					},
					{ RTCSignalingState::HaveLocalOffer,
						webrtc::PeerConnectionInterface::kHaveLocalOffer },
						{ RTCSignalingState::HaveRemoteOffer,
							webrtc::PeerConnectionInterface::kHaveRemoteOffer },
							{ RTCSignalingState::HaveLocalPranswer,
								webrtc::PeerConnectionInterface::kHaveLocalPrAnswer },
								{ RTCSignalingState::HaveRemotePranswer,
									webrtc::PeerConnectionInterface::kHaveRemotePrAnswer },
									{ RTCSignalingState::Closed,
										webrtc::PeerConnectionInterface::kClosed },
							DEFINE_MARSHALLED_ENUM_END(SignalingState)

							DEFINE_MARSHALLED_ENUM(DataChannelState,
								RTCDataChannelState,
								webrtc::DataChannelInterface::DataState)
						{
							RTCDataChannelState::Connecting,
								webrtc::DataChannelInterface::kConnecting
						},
						{ RTCDataChannelState::Open,
							webrtc::DataChannelInterface::kOpen },
							{ RTCDataChannelState::Closing,
								webrtc::DataChannelInterface::kClosing },
								{ RTCDataChannelState::Closed,
									webrtc::DataChannelInterface::kClosed },
								DEFINE_MARSHALLED_ENUM_END(DataChannelState)

								DEFINE_MARSHALLED_ENUM(IceGatheringState,
									RTCIceGatheringState, webrtc::PeerConnectionInterface::IceGatheringState)
							{
								RTCIceGatheringState::New,
									webrtc::PeerConnectionInterface::kIceGatheringNew
							},
							{ RTCIceGatheringState::Gathering,
								webrtc::PeerConnectionInterface::kIceGatheringGathering },
								{ RTCIceGatheringState::Complete,
									webrtc::PeerConnectionInterface::kIceGatheringComplete },
									DEFINE_MARSHALLED_ENUM_END(IceGatheringState)

									DEFINE_MARSHALLED_ENUM(IceConnectionState,
										RTCIceConnectionState, webrtc::PeerConnectionInterface::IceConnectionState)
								{
									RTCIceConnectionState::New,
										webrtc::PeerConnectionInterface::kIceConnectionNew
								},
								{ RTCIceConnectionState::Checking,
									webrtc::PeerConnectionInterface::kIceConnectionChecking },
									{ RTCIceConnectionState::Connected,
										webrtc::PeerConnectionInterface::kIceConnectionConnected },
										{ RTCIceConnectionState::Completed,
											webrtc::PeerConnectionInterface::kIceConnectionCompleted },
											{ RTCIceConnectionState::Failed,
												webrtc::PeerConnectionInterface::kIceConnectionFailed },
												{ RTCIceConnectionState::Disconnected,
													webrtc::PeerConnectionInterface::kIceConnectionDisconnected },
													{ RTCIceConnectionState::Closed,
														webrtc::PeerConnectionInterface::kIceConnectionClosed },
										DEFINE_MARSHALLED_ENUM_END(IceConnectionState)

										DEFINE_MARSHALLED_ENUM(StatsType,
											RTCStatsType, webrtc::StatsReport::StatsType)
									{
										RTCStatsType::StatsReportTypeSession,
											webrtc::StatsReport::kStatsReportTypeSession
									},
									{ RTCStatsType::StatsReportTypeTransport,
										webrtc::StatsReport::kStatsReportTypeTransport },
										{ RTCStatsType::StatsReportTypeComponent,
											webrtc::StatsReport::kStatsReportTypeComponent },
											{ RTCStatsType::StatsReportTypeCandidatePair,
												webrtc::StatsReport::kStatsReportTypeCandidatePair },
												{ RTCStatsType::StatsReportTypeBwe,
													webrtc::StatsReport::kStatsReportTypeBwe },
													{ RTCStatsType::StatsReportTypeSsrc,
														webrtc::StatsReport::kStatsReportTypeSsrc },
														{ RTCStatsType::StatsReportTypeRemoteSsrc,
															webrtc::StatsReport::kStatsReportTypeRemoteSsrc },
															{ RTCStatsType::StatsReportTypeTrack,
																webrtc::StatsReport::kStatsReportTypeTrack },
																{ RTCStatsType::StatsReportTypeIceLocalCandidate,
																	webrtc::StatsReport::kStatsReportTypeIceLocalCandidate },
																	{ RTCStatsType::StatsReportTypeIceRemoteCandidate,
																		webrtc::StatsReport::kStatsReportTypeIceRemoteCandidate },
																		{ RTCStatsType::StatsReportTypeCertificate,
																			webrtc::StatsReport::kStatsReportTypeCertificate },
																			{ RTCStatsType::StatsReportTypeDataChannel,
																				webrtc::StatsReport::kStatsReportTypeDataChannel },
											DEFINE_MARSHALLED_ENUM_END(StatsType)

											DEFINE_MARSHALLED_ENUM(StatsValueName,
												RTCStatsValueName, webrtc::StatsReport::StatsValueName)
										{
											RTCStatsValueName::StatsValueNameActiveConnection,
												webrtc::StatsReport::kStatsValueNameActiveConnection
										},
										{ RTCStatsValueName::StatsValueNameAudioInputLevel,
											webrtc::StatsReport::kStatsValueNameAudioInputLevel },
											{ RTCStatsValueName::StatsValueNameAudioOutputLevel,
												webrtc::StatsReport::kStatsValueNameAudioOutputLevel },
												{ RTCStatsValueName::StatsValueNameBytesReceived,
													webrtc::StatsReport::kStatsValueNameBytesReceived },
													{ RTCStatsValueName::StatsValueNameBytesSent,
														webrtc::StatsReport::kStatsValueNameBytesSent },
														{ RTCStatsValueName::StatsValueNameCodecImplementationName,
															webrtc::StatsReport::kStatsValueNameCodecImplementationName },
															{ RTCStatsValueName::StatsValueNameDataChannelId,
																webrtc::StatsReport::kStatsValueNameDataChannelId },
																{ RTCStatsValueName::StatsValueNameMediaType,
																	webrtc::StatsReport::kStatsValueNameMediaType },
																	{ RTCStatsValueName::StatsValueNamePacketsLost,
																		webrtc::StatsReport::kStatsValueNamePacketsLost },
																		{ RTCStatsValueName::StatsValueNamePacketsReceived,
																			webrtc::StatsReport::kStatsValueNamePacketsReceived },
																			{ RTCStatsValueName::StatsValueNamePacketsSent,
																				webrtc::StatsReport::kStatsValueNamePacketsSent },
																				{ RTCStatsValueName::StatsValueNameProtocol,
																					webrtc::StatsReport::kStatsValueNameProtocol },
																					{ RTCStatsValueName::StatsValueNameReceiving,
																						webrtc::StatsReport::kStatsValueNameReceiving },
																						{ RTCStatsValueName::StatsValueNameSelectedCandidatePairId,
																							webrtc::StatsReport::kStatsValueNameSelectedCandidatePairId },
																							{ RTCStatsValueName::StatsValueNameSsrc,
																								webrtc::StatsReport::kStatsValueNameSsrc },
																								{ RTCStatsValueName::StatsValueNameState,
																									webrtc::StatsReport::kStatsValueNameState },
																									{ RTCStatsValueName::StatsValueNameTransportId,
																										webrtc::StatsReport::kStatsValueNameTransportId },
																										{ RTCStatsValueName::StatsValueNameAccelerateRate,
																											webrtc::StatsReport::kStatsValueNameAccelerateRate },
																											{ RTCStatsValueName::StatsValueNameActualEncBitrate,
																												webrtc::StatsReport::kStatsValueNameActualEncBitrate },
																												{ RTCStatsValueName::StatsValueNameAdaptationChanges,
																													webrtc::StatsReport::kStatsValueNameAdaptationChanges },
																													{ RTCStatsValueName::StatsValueNameAvailableReceiveBandwidth,
																														webrtc::StatsReport::kStatsValueNameAvailableReceiveBandwidth },
																														{ RTCStatsValueName::StatsValueNameAvailableSendBandwidth,
																															webrtc::StatsReport::kStatsValueNameAvailableSendBandwidth },
																															{ RTCStatsValueName::StatsValueNameAvgEncodeMs,
																																webrtc::StatsReport::kStatsValueNameAvgEncodeMs },
																																{ RTCStatsValueName::StatsValueNameBandwidthLimitedResolution,
																																	webrtc::StatsReport::kStatsValueNameBandwidthLimitedResolution },
																																	{ RTCStatsValueName::StatsValueNameBucketDelay,
																																		webrtc::StatsReport::kStatsValueNameBucketDelay },
																																		{ RTCStatsValueName::StatsValueNameCaptureStartNtpTimeMs,
																																			webrtc::StatsReport::kStatsValueNameCaptureStartNtpTimeMs },
																																			{ RTCStatsValueName::StatsValueNameCandidateIPAddress,
																																				webrtc::StatsReport::kStatsValueNameCandidateIPAddress },
																																				{ RTCStatsValueName::StatsValueNameCandidateNetworkType,
																																					webrtc::StatsReport::kStatsValueNameCandidateNetworkType },
																																					{ RTCStatsValueName::StatsValueNameCandidatePortNumber,
																																						webrtc::StatsReport::kStatsValueNameCandidatePortNumber },
																																						{ RTCStatsValueName::StatsValueNameCandidatePriority,
																																							webrtc::StatsReport::kStatsValueNameCandidatePriority },
																																							{ RTCStatsValueName::StatsValueNameCandidateTransportType,
																																								webrtc::StatsReport::kStatsValueNameCandidateTransportType },
																																								{ RTCStatsValueName::StatsValueNameCandidateType,
																																									webrtc::StatsReport::kStatsValueNameCandidateType },
																																									{ RTCStatsValueName::StatsValueNameChannelId,
																																										webrtc::StatsReport::kStatsValueNameChannelId },
																																										{ RTCStatsValueName::StatsValueNameCodecName,
																																											webrtc::StatsReport::kStatsValueNameCodecName },
																																											{ RTCStatsValueName::StatsValueNameComponent,
																																												webrtc::StatsReport::kStatsValueNameComponent },
																																												{ RTCStatsValueName::StatsValueNameContentName,
																																													webrtc::StatsReport::kStatsValueNameContentName },
																																													{ RTCStatsValueName::StatsValueNameCpuLimitedResolution,
																																														webrtc::StatsReport::kStatsValueNameCpuLimitedResolution },
																																														{ RTCStatsValueName::StatsValueNameCurrentDelayMs,
																																															webrtc::StatsReport::kStatsValueNameCurrentDelayMs },
																																															{ RTCStatsValueName::StatsValueNameDecodeMs,
																																																webrtc::StatsReport::kStatsValueNameDecodeMs },
																																																{ RTCStatsValueName::StatsValueNameDecodingCNG,
																																																	webrtc::StatsReport::kStatsValueNameDecodingCNG },
																																																	{ RTCStatsValueName::StatsValueNameDecodingCTN,
																																																		webrtc::StatsReport::kStatsValueNameDecodingCTN },
																																																		{ RTCStatsValueName::StatsValueNameDecodingCTSG,
																																																			webrtc::StatsReport::kStatsValueNameDecodingCTSG },
																																																			{ RTCStatsValueName::StatsValueNameDecodingNormal,
																																																				webrtc::StatsReport::kStatsValueNameDecodingNormal },
																																																				{ RTCStatsValueName::StatsValueNameDecodingPLC,
																																																					webrtc::StatsReport::kStatsValueNameDecodingPLC },
																																																					{ RTCStatsValueName::StatsValueNameDecodingPLCCNG,
																																																						webrtc::StatsReport::kStatsValueNameDecodingPLCCNG },
																																																						{ RTCStatsValueName::StatsValueNameDer,
																																																							webrtc::StatsReport::kStatsValueNameDer },
																																																							{ RTCStatsValueName::StatsValueNameDtlsCipher,
																																																								webrtc::StatsReport::kStatsValueNameDtlsCipher },
																																																								{ RTCStatsValueName::StatsValueNameEchoCancellationQualityMin,
																																																									webrtc::StatsReport::kStatsValueNameEchoCancellationQualityMin },
																																																									{ RTCStatsValueName::StatsValueNameEchoDelayMedian,
																																																										webrtc::StatsReport::kStatsValueNameEchoDelayMedian },
																																																										{ RTCStatsValueName::StatsValueNameEchoDelayStdDev,
																																																											webrtc::StatsReport::kStatsValueNameEchoDelayStdDev },
																																																											{ RTCStatsValueName::StatsValueNameEchoReturnLoss,
																																																												webrtc::StatsReport::kStatsValueNameEchoReturnLoss },
																																																												{ RTCStatsValueName::StatsValueNameEchoReturnLossEnhancement,
																																																													webrtc::StatsReport::kStatsValueNameEchoReturnLossEnhancement },
																																																													{ RTCStatsValueName::StatsValueNameEncodeUsagePercent,
																																																														webrtc::StatsReport::kStatsValueNameEncodeUsagePercent },
																																																														{ RTCStatsValueName::StatsValueNameExpandRate,
																																																															webrtc::StatsReport::kStatsValueNameExpandRate },
																																																															{ RTCStatsValueName::StatsValueNameFingerprint,
																																																																webrtc::StatsReport::kStatsValueNameFingerprint },
																																																																{ RTCStatsValueName::StatsValueNameFingerprintAlgorithm,
																																																																	webrtc::StatsReport::kStatsValueNameFingerprintAlgorithm },
																																																																	{ RTCStatsValueName::StatsValueNameFirsReceived,
																																																																		webrtc::StatsReport::kStatsValueNameFirsReceived },
																																																																		{ RTCStatsValueName::StatsValueNameFirsSent,
																																																																			webrtc::StatsReport::kStatsValueNameFirsSent },
																																																																			{ RTCStatsValueName::StatsValueNameFrameHeightInput,
																																																																				webrtc::StatsReport::kStatsValueNameFrameHeightInput },
																																																																				{ RTCStatsValueName::StatsValueNameFrameHeightReceived,
																																																																					webrtc::StatsReport::kStatsValueNameFrameHeightReceived },
																																																																					{ RTCStatsValueName::StatsValueNameFrameHeightSent,
																																																																						webrtc::StatsReport::kStatsValueNameFrameHeightSent },
																																																																						{ RTCStatsValueName::StatsValueNameFrameRateDecoded,
																																																																							webrtc::StatsReport::kStatsValueNameFrameRateDecoded },
																																																																							{ RTCStatsValueName::StatsValueNameFrameRateInput,
																																																																								webrtc::StatsReport::kStatsValueNameFrameRateInput },
																																																																								{ RTCStatsValueName::StatsValueNameFrameRateOutput,
																																																																									webrtc::StatsReport::kStatsValueNameFrameRateOutput },
																																																																									{ RTCStatsValueName::StatsValueNameFrameRateReceived,
																																																																										webrtc::StatsReport::kStatsValueNameFrameRateReceived },
																																																																										{ RTCStatsValueName::StatsValueNameFrameRateSent,
																																																																											webrtc::StatsReport::kStatsValueNameFrameRateSent },
																																																																											{ RTCStatsValueName::StatsValueNameFrameWidthInput,
																																																																												webrtc::StatsReport::kStatsValueNameFrameWidthInput },
																																																																												{ RTCStatsValueName::StatsValueNameFrameWidthReceived,
																																																																													webrtc::StatsReport::kStatsValueNameFrameWidthReceived },
																																																																													{ RTCStatsValueName::StatsValueNameFrameWidthSent,
																																																																														webrtc::StatsReport::kStatsValueNameFrameWidthSent },
																																																																														{ RTCStatsValueName::StatsValueNameInitiator,
																																																																															webrtc::StatsReport::kStatsValueNameInitiator },
																																																																															{ RTCStatsValueName::StatsValueNameIssuerId,
																																																																																webrtc::StatsReport::kStatsValueNameIssuerId },
																																																																																{ RTCStatsValueName::StatsValueNameJitterBufferMs,
																																																																																	webrtc::StatsReport::kStatsValueNameJitterBufferMs },
																																																																																	{ RTCStatsValueName::StatsValueNameJitterReceived,
																																																																																		webrtc::StatsReport::kStatsValueNameJitterReceived },
																																																																																		{ RTCStatsValueName::StatsValueNameLabel,
																																																																																			webrtc::StatsReport::kStatsValueNameLabel },
																																																																																			{ RTCStatsValueName::StatsValueNameLocalAddress,
																																																																																				webrtc::StatsReport::kStatsValueNameLocalAddress },
																																																																																				{ RTCStatsValueName::StatsValueNameLocalCandidateId,
																																																																																					webrtc::StatsReport::kStatsValueNameLocalCandidateId },
																																																																																					{ RTCStatsValueName::StatsValueNameLocalCandidateType,
																																																																																						webrtc::StatsReport::kStatsValueNameLocalCandidateType },
																																																																																						{ RTCStatsValueName::StatsValueNameLocalCertificateId,
																																																																																							webrtc::StatsReport::kStatsValueNameLocalCertificateId },
																																																																																							{ RTCStatsValueName::StatsValueNameMaxDecodeMs,
																																																																																								webrtc::StatsReport::kStatsValueNameMaxDecodeMs },
																																																																																								{ RTCStatsValueName::StatsValueNameMinPlayoutDelayMs,
																																																																																									webrtc::StatsReport::kStatsValueNameMinPlayoutDelayMs },
																																																																																									{ RTCStatsValueName::StatsValueNameNacksReceived,
																																																																																										webrtc::StatsReport::kStatsValueNameNacksReceived },
																																																																																										{ RTCStatsValueName::StatsValueNameNacksSent,
																																																																																											webrtc::StatsReport::kStatsValueNameNacksSent },
																																																																																											{ RTCStatsValueName::StatsValueNamePlisReceived,
																																																																																												webrtc::StatsReport::kStatsValueNamePlisReceived },
																																																																																												{ RTCStatsValueName::StatsValueNamePlisSent,
																																																																																													webrtc::StatsReport::kStatsValueNamePlisSent },
																																																																																													{ RTCStatsValueName::StatsValueNamePreemptiveExpandRate,
																																																																																														webrtc::StatsReport::kStatsValueNamePreemptiveExpandRate },
																																																																																														{ RTCStatsValueName::StatsValueNamePreferredJitterBufferMs,
																																																																																															webrtc::StatsReport::kStatsValueNamePreferredJitterBufferMs },
																																																																																															{ RTCStatsValueName::StatsValueNameRemoteAddress,
																																																																																																webrtc::StatsReport::kStatsValueNameRemoteAddress },
																																																																																																{ RTCStatsValueName::StatsValueNameRemoteCandidateId,
																																																																																																	webrtc::StatsReport::kStatsValueNameRemoteCandidateId },
																																																																																																	{ RTCStatsValueName::StatsValueNameRemoteCandidateType,
																																																																																																		webrtc::StatsReport::kStatsValueNameRemoteCandidateType },
																																																																																																		{ RTCStatsValueName::StatsValueNameRemoteCertificateId,
																																																																																																			webrtc::StatsReport::kStatsValueNameRemoteCertificateId },
																																																																																																			{ RTCStatsValueName::StatsValueNameRenderDelayMs,
																																																																																																				webrtc::StatsReport::kStatsValueNameRenderDelayMs },
																																																																																																				{ RTCStatsValueName::StatsValueNameRetransmitBitrate,
																																																																																																					webrtc::StatsReport::kStatsValueNameRetransmitBitrate },
																																																																																																					{ RTCStatsValueName::StatsValueNameRtt,
																																																																																																						webrtc::StatsReport::kStatsValueNameRtt },
																																																																																																						{ RTCStatsValueName::StatsValueNameSecondaryDecodedRate,
																																																																																																							webrtc::StatsReport::kStatsValueNameSecondaryDecodedRate },
																																																																																																							{ RTCStatsValueName::StatsValueNameSendPacketsDiscarded,
																																																																																																								webrtc::StatsReport::kStatsValueNameSendPacketsDiscarded },
																																																																																																								{ RTCStatsValueName::StatsValueNameSpeechExpandRate,
																																																																																																									webrtc::StatsReport::kStatsValueNameSpeechExpandRate },
																																																																																																									{ RTCStatsValueName::StatsValueNameSrtpCipher,
																																																																																																										webrtc::StatsReport::kStatsValueNameSrtpCipher },
																																																																																																										{ RTCStatsValueName::StatsValueNameTargetDelayMs,
																																																																																																											webrtc::StatsReport::kStatsValueNameTargetDelayMs },
																																																																																																											{ RTCStatsValueName::StatsValueNameTargetEncBitrate,
																																																																																																												webrtc::StatsReport::kStatsValueNameTargetEncBitrate },
																																																																																																												{ RTCStatsValueName::StatsValueNameTrackId,
																																																																																																													webrtc::StatsReport::kStatsValueNameTrackId },
																																																																																																													{ RTCStatsValueName::StatsValueNameTransmitBitrate,
																																																																																																														webrtc::StatsReport::kStatsValueNameTransmitBitrate },
																																																																																																														{ RTCStatsValueName::StatsValueNameTransportType,
																																																																																																															webrtc::StatsReport::kStatsValueNameTransportType },
																																																																																																															{ RTCStatsValueName::StatsValueNameTypingNoiseState,
																																																																																																																webrtc::StatsReport::kStatsValueNameTypingNoiseState },
																																																																																																																{ RTCStatsValueName::StatsValueNameViewLimitedResolution,
																																																																																																																	webrtc::StatsReport::kStatsValueNameViewLimitedResolution },
																																																																																																																	{ RTCStatsValueName::StatsValueNameWritable,
																																																																																																																		webrtc::StatsReport::kStatsValueNameWritable },
																																																																																																																		{ RTCStatsValueName::StatsValueNameCurrentEndToEndDelayMs,
																																																																																																																			webrtc::StatsReport::kStatsValueNameCurrentEndToEndDelayMs }
												DEFINE_MARSHALLED_ENUM_END(StatsValueName)

												std::string FromCx(String^ inObj) {
												return rtc::ToUtf8(inObj->Data());
											}

											String^ ToCx(std::string const& inObj) {
												return ref new String(rtc::ToUtf16(inObj).c_str());
											}

											void FromCx(
												RTCIceServer^ inObj,
												webrtc::PeerConnectionInterface::IceServer* outObj) {
												if (inObj->Url != nullptr)
													outObj->uri = rtc::ToUtf8(inObj->Url->Data());
												if (inObj->Username != nullptr)
													outObj->username = rtc::ToUtf8(inObj->Username->Data());
												if (inObj->Credential != nullptr)
													outObj->password = rtc::ToUtf8(inObj->Credential->Data());
											}

											void FromCx(
												RTCConfiguration^ inObj,
												webrtc::PeerConnectionInterface::RTCConfiguration* outObj) {

												// BundlePolicy
												if (inObj->BundlePolicy == nullptr) {
													// Default as defined in the WebApi spec.
													outObj->bundle_policy =
														webrtc::PeerConnectionInterface::kBundlePolicyBalanced;
												}
												else {
													FromCx(inObj->BundlePolicy->Value, &outObj->bundle_policy);
												}

												// IceTransportPolicy
												if (inObj->IceTransportPolicy == nullptr) {
													// Default as defined in the WebApi spec.
													outObj->type = webrtc::PeerConnectionInterface::kAll;
												}
												else {
													FromCx(inObj->IceTransportPolicy->Value, &outObj->type);
												}

												// IceServers
												if (inObj->IceServers != nullptr) {
													FromCx(inObj->IceServers, &outObj->servers);
												}
											}

											void FromCx(
												Org::WebRtc::RTCDataChannelInit^ inObj,
												webrtc::DataChannelInit* outObj) {
												// Use ternary operators to handle default values.
												outObj->ordered = inObj->Ordered != nullptr ? inObj->Ordered->Value : true;
												outObj->maxRetransmitTime = inObj->MaxPacketLifeTime != nullptr ?
													inObj->MaxPacketLifeTime->Value : -1;
												outObj->maxRetransmits = inObj->MaxRetransmits != nullptr ?
													inObj->MaxRetransmits->Value : -1;
												outObj->protocol = FromCx(inObj->Protocol);
												outObj->negotiated = inObj->Negotiated != nullptr ?
													inObj->Negotiated->Value : false;
												outObj->id = inObj->Id != nullptr ? inObj->Id->Value : -1;
											}

											void FromCx(
												Org::WebRtc::RTCIceCandidate^ inObj,
												rtc::scoped_ptr<webrtc::IceCandidateInterface>* outObj) {
												outObj->reset(webrtc::CreateIceCandidate(
													FromCx(inObj->SdpMid),
													inObj->SdpMLineIndex,
													FromCx(inObj->Candidate), nullptr));
											}

											void ToCx(
												webrtc::IceCandidateInterface const& inObj,
												Org::WebRtc::RTCIceCandidate^* outObj) {
												(*outObj) = ref new Org::WebRtc::RTCIceCandidate();
												(*outObj)->Candidate = ToCx(webrtc::SdpSerializeCandidate(inObj));
												(*outObj)->SdpMid = ToCx(inObj.sdp_mid());
												(*outObj)->SdpMLineIndex = inObj.sdp_mline_index();
											}

											std::string FromCx(
												Org::WebRtc::RTCSdpType const& inObj) {
												if (inObj == Org::WebRtc::RTCSdpType::Offer)
													return webrtc::SessionDescriptionInterface::kOffer;
												if (inObj == Org::WebRtc::RTCSdpType::Answer)
													return webrtc::SessionDescriptionInterface::kAnswer;
												if (inObj == Org::WebRtc::RTCSdpType::Pranswer)
													return webrtc::SessionDescriptionInterface::kPrAnswer;

												throw ref new Platform::NotImplementedException();
											}

											void ToCx(
												std::string const& inObj,
												Org::WebRtc::RTCSdpType* outObj) {
												if (inObj == webrtc::SessionDescriptionInterface::kOffer)
													(*outObj) = Org::WebRtc::RTCSdpType::Offer;
												if (inObj == webrtc::SessionDescriptionInterface::kAnswer)
													(*outObj) = Org::WebRtc::RTCSdpType::Answer;
												if (inObj == webrtc::SessionDescriptionInterface::kPrAnswer)
													(*outObj) = Org::WebRtc::RTCSdpType::Pranswer;
											}

											void FromCx(
												Org::WebRtc::RTCSessionDescription^ inObj,
												rtc::scoped_ptr<webrtc::SessionDescriptionInterface>* outObj) {
												outObj->reset(webrtc::CreateSessionDescription(
													FromCx(inObj->Type->Value),
													FromCx(inObj->Sdp), nullptr));
											}

											void ToCx(
												const webrtc::SessionDescriptionInterface* inObj,
												Org::WebRtc::RTCSessionDescription^* outObj) {
												(*outObj) = ref new Org::WebRtc::RTCSessionDescription();

												std::string sdp;
												inObj->ToString(&sdp);
												(*outObj)->Sdp = ToCx(sdp);

												Org::WebRtc::RTCSdpType type;
												ToCx(inObj->type(), &type);
												(*outObj)->Type = type;
											}

											void ToCx(
												const webrtc::StatsReport* inObj,
												Org::WebRtc::RTCStatsReport^* outObj) {
												(*outObj) = ref new Org::WebRtc::RTCStatsReport();

												if (inObj->id() != nullptr) {
													(*outObj)->ReportId = ToCx(inObj->id()->ToString());
												}
												(*outObj)->Timestamp = inObj->timestamp();

												Org::WebRtc::RTCStatsType type;
												ToCx(inObj->id()->type(), &type);
												(*outObj)->StatsType = type;

												for (auto value : inObj->values()) {
													Org::WebRtc::RTCStatsValueName stat_name;
													ToCx(value.first, &stat_name);
													Platform::Object^ val = nullptr;
													switch (value.second->type()) {
													case webrtc::StatsReport::Value::kInt:
														val = value.second->int_val();
														break;
													case webrtc::StatsReport::Value::kInt64:
														val = value.second->int64_val();
														break;
													case webrtc::StatsReport::Value::kFloat:
														val = value.second->float_val();
														break;
													case webrtc::StatsReport::Value::kBool:
														val = value.second->bool_val();
														break;
													case webrtc::StatsReport::Value::kStaticString: {
														val = ToCx(value.second->static_string_val());
													}
																																					break;
													case webrtc::StatsReport::Value::kString: {
														val = ToCx(value.second->string_val().c_str());
													}
																																		break;
													default:
														break;
													}

													if (val != nullptr) {
														(*outObj)->Values->Insert(stat_name, val);
													}
												}
											}
		}
	}
}  // namespace Org.WebRtc.Internal

