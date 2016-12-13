// Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef WEBRTC_BUILD_WINRT_GYP_API_RTCSTATSREPORT_H_
#define WEBRTC_BUILD_WINRT_GYP_API_RTCSTATSREPORT_H_

#include <collection.h>
#include <webrtc/api/statstypes.h>

#include <vector>
#include <map>


using Windows::Foundation::Collections::IVector;
using Windows::Foundation::Collections::IMap;
using Platform::Collections::Map;

namespace Org {
	namespace WebRtc {

		public enum class RTCStatsValueName {
			StatsValueNameActiveConnection,
			StatsValueNameAudioInputLevel,
			StatsValueNameAudioOutputLevel,
			StatsValueNameBytesReceived,
			StatsValueNameBytesSent,
			StatsValueNameCodecImplementationName,
			StatsValueNameDataChannelId,
			StatsValueNameMediaType,
			StatsValueNamePacketsLost,
			StatsValueNamePacketsReceived,
			StatsValueNamePacketsSent,
			StatsValueNameProtocol,
			StatsValueNameReceiving,
			StatsValueNameSelectedCandidatePairId,
			StatsValueNameSsrc,
			StatsValueNameState,
			StatsValueNameTransportId,

			// Internal StatsValue names.
			StatsValueNameAccelerateRate,
			StatsValueNameActualEncBitrate,
			StatsValueNameAdaptationChanges,
			StatsValueNameAvailableReceiveBandwidth,
			StatsValueNameAvailableSendBandwidth,
			StatsValueNameAvgEncodeMs,
			StatsValueNameBandwidthLimitedResolution,
			StatsValueNameBucketDelay,
			StatsValueNameCaptureStartNtpTimeMs,
			StatsValueNameCandidateIPAddress,
			StatsValueNameCandidateNetworkType,
			StatsValueNameCandidatePortNumber,
			StatsValueNameCandidatePriority,
			StatsValueNameCandidateTransportType,
			StatsValueNameCandidateType,
			StatsValueNameChannelId,
			StatsValueNameCodecName,
			StatsValueNameComponent,
			StatsValueNameContentName,
			StatsValueNameCpuLimitedResolution,
			StatsValueNameCurrentDelayMs,
			StatsValueNameDecodeMs,
			StatsValueNameDecodingCNG,
			StatsValueNameDecodingCTN,
			StatsValueNameDecodingCTSG,
			StatsValueNameDecodingNormal,
			StatsValueNameDecodingPLC,
			StatsValueNameDecodingPLCCNG,
			StatsValueNameDer,
			StatsValueNameDtlsCipher,
			StatsValueNameEchoCancellationQualityMin,
			StatsValueNameEchoDelayMedian,
			StatsValueNameEchoDelayStdDev,
			StatsValueNameEchoReturnLoss,
			StatsValueNameEchoReturnLossEnhancement,
			StatsValueNameEncodeUsagePercent,
			StatsValueNameExpandRate,
			StatsValueNameFingerprint,
			StatsValueNameFingerprintAlgorithm,
			StatsValueNameFirsReceived,
			StatsValueNameFirsSent,
			StatsValueNameFrameHeightInput,
			StatsValueNameFrameHeightReceived,
			StatsValueNameFrameHeightSent,
			StatsValueNameFrameRateDecoded,
			StatsValueNameFrameRateInput,
			StatsValueNameFrameRateOutput,
			StatsValueNameFrameRateReceived,
			StatsValueNameFrameRateSent,
			StatsValueNameFrameWidthInput,
			StatsValueNameFrameWidthReceived,
			StatsValueNameFrameWidthSent,
			StatsValueNameInitiator,
			StatsValueNameIssuerId,
			StatsValueNameJitterBufferMs,
			StatsValueNameJitterReceived,
			StatsValueNameLabel,
			StatsValueNameLocalAddress,
			StatsValueNameLocalCandidateId,
			StatsValueNameLocalCandidateType,
			StatsValueNameLocalCertificateId,
			StatsValueNameMaxDecodeMs,
			StatsValueNameMinPlayoutDelayMs,
			StatsValueNameNacksReceived,
			StatsValueNameNacksSent,
			StatsValueNamePlisReceived,
			StatsValueNamePlisSent,
			StatsValueNamePreemptiveExpandRate,
			StatsValueNamePreferredJitterBufferMs,
			StatsValueNameRemoteAddress,
			StatsValueNameRemoteCandidateId,
			StatsValueNameRemoteCandidateType,
			StatsValueNameRemoteCertificateId,
			StatsValueNameRenderDelayMs,
			StatsValueNameRetransmitBitrate,
			StatsValueNameRtt,
			StatsValueNameSecondaryDecodedRate,
			StatsValueNameSendPacketsDiscarded,
			StatsValueNameSpeechExpandRate,
			StatsValueNameSrtpCipher,
			StatsValueNameTargetDelayMs,
			StatsValueNameTargetEncBitrate,
			StatsValueNameTrackId,
			StatsValueNameTransmitBitrate,
			StatsValueNameTransportType,
			StatsValueNameTypingNoiseState,
			StatsValueNameViewLimitedResolution,
			StatsValueNameWritable,
			StatsValueNameCurrentEndToEndDelayMs,
		};

		public enum class RTCStatsType {
			// StatsReport types.
			// A StatsReport of |type| = "googSession" contains overall information
			// about the thing libjingle calls a session (which may contain one
			// or more RTP sessions.
			StatsReportTypeSession,

			// A StatsReport of |type| = "googTransport" contains information
			// about a libjingle "transport".
			StatsReportTypeTransport,

			// A StatsReport of |type| = "googComponent" contains information
			// about a libjingle "channel" (typically, RTP or RTCP for a transport).
			// This is intended to be the same thing as an ICE "Component".
			StatsReportTypeComponent,

			// A StatsReport of |type| = "googCandidatePair" contains information
			// about a libjingle "connection" - a single source/destination port pair.
			// This is intended to be the same thing as an ICE "candidate pair".
			StatsReportTypeCandidatePair,

			// A StatsReport of |type| = "VideoBWE" is statistics for video Bandwidth
			// Estimation, which is global per-session.  The |id| field is "bweforvideo"
			// (will probably change in the future).
			StatsReportTypeBwe,

			// A StatsReport of |type| = "ssrc" is statistics for a specific rtp stream.
			// The |id| field is the SSRC in decimal form of the rtp stream.
			StatsReportTypeSsrc,

			// A StatsReport of |type| = "remoteSsrc" is statistics for a specific
			// rtp stream, generated by the remote end of the connection.
			StatsReportTypeRemoteSsrc,

			// A StatsReport of |type| = "googTrack" is statistics for a specific media
			// track. The |id| field is the track id.
			StatsReportTypeTrack,

			// A StatsReport of |type| = "localcandidate" or "remotecandidate" is
			// attributes on a specific ICE Candidate. It links to its connection pair
			// by candidate id. The string value is taken from
			// http://w3c.github.io/webrtc-stats/#rtcstatstype-enum*.
			StatsReportTypeIceLocalCandidate,
			StatsReportTypeIceRemoteCandidate,

			// A StatsReport of |type| = "googCertificate" contains an SSL certificate
			// transmitted by one of the endpoints of this connection.  The |id| is
			// controlled by the fingerprint, and is used to identify the certificate in
			// the Channel stats (as "googLocalCertificateId" or
			// "googRemoteCertificateId") and in any child certificates (as
			// "googIssuerId").
			StatsReportTypeCertificate,

			// A StatsReport of |type| = "datachannel" with statistics for a
			// particular DataChannel.
			StatsReportTypeDataChannel,
		};

		typedef IMap< RTCStatsValueName, Platform::Object^>^ RTCStatsValues;

		/// <summary>
		/// CX object of webrtc::StatsReport
		/// </summary>
		public ref class RTCStatsReport sealed {
		public:
			RTCStatsReport() {
				Values = ref new Platform::Collections::Map<
					RTCStatsValueName, Platform::Object^>();
			}

			property Platform::String^ ReportId;
			property double Timestamp;  // Time since 1970-01-01T00:00:00Z
																	// in milliseconds
			property RTCStatsType StatsType;
			property RTCStatsValues Values;
		};

		typedef IVector<RTCStatsReport^>^ RTCStatsReports;
	} // namespace WebRtc
}  // namespace Org

#endif  //  WEBRTC_BUILD_WINRT_GYP_API_RTCSTATSREPORT_H_
