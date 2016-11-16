
// Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef WEBRTC_BUILD_WINRT_GYP_API_GLOBALOBSERVER_H_
#define WEBRTC_BUILD_WINRT_GYP_API_GLOBALOBSERVER_H_

#include <ppltasks.h>
#include <functional>
#include <string>
#include "webrtc/api/peerconnectioninterface.h"
#include "webrtc/base/event.h"
#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/build/WinRT_gyp/stats/webrtc_stats_observer.h"

namespace Org {
	namespace WebRtc {
		ref class RTCPeerConnection;
		ref class RTCDataChannel;
	}
}  // namespace Org::WebRtc

namespace Org {
	namespace WebRtc {
		namespace Internal {

			class GlobalObserver : public webrtc::PeerConnectionObserver,
				public webrtc::WebRTCStatsObserverWinRT {
			public:
				GlobalObserver();

				void SetPeerConnection(Org::WebRtc::RTCPeerConnection^ pc);

				void EnableETWStats(bool enable);
				bool AreETWStatsEnabled();

				void EnableConnectionHealthStats(bool enable);
				bool AreConnectionHealthStatsEnabled();

				void EnableRTCStats(bool enable);
				bool AreRTCStatsEnabled();

				void EnableSendRtcStatsToRemoteHost(bool enable);
				bool IsSendRtcStatsToRemoteHostEnabled();
				void SetRtcStatsDestinationHost(std::string hostname);
				std::string GetRtcStatsDestinationHost();
				void SetRtcStatsDestinationPort(int port);
				int GetRtcStatsDestinationPort();

				// PeerConnectionObserver functions
				virtual void OnSignalingChange(
					webrtc::PeerConnectionInterface::SignalingState new_state);

				virtual void OnStateChange(StateType state_changed);

				virtual void OnAddStream(webrtc::MediaStreamInterface* stream);

				virtual void OnRemoveStream(webrtc::MediaStreamInterface* stream);

				virtual void OnDataChannel(webrtc::DataChannelInterface* data_channel);

				virtual void OnRenegotiationNeeded();

				virtual void OnIceConnectionChange(
					webrtc::PeerConnectionInterface::IceConnectionState new_state);

				virtual void OnIceGatheringChange(
					webrtc::PeerConnectionInterface::IceGatheringState new_state);

				virtual void OnIceCandidate(const webrtc::IceCandidateInterface* candidate);

				virtual void OnIceComplete();

				// WebRTCStatsObserverWinRT functions
				virtual void OnConnectionHealthStats(
					const webrtc::ConnectionHealthStats& stats);
				virtual void OnRTCStatsReportsReady(
					const Org::WebRtc::RTCStatsReports& rtcStatsReports);

			private:
				void ResetStatsConfig();

			private:
				Org::WebRtc::RTCPeerConnection^ _pc;
				rtc::scoped_refptr<webrtc::WebRTCStatsObserver> _stats_observer;
				bool _etwStatsEnabled;
				bool _connectionHealthStatsEnabled;
				bool _rtcStatsEnabled;

				bool _sendRtcStatsToRemoteHostEnabled;
				std::string _rtcStatsDestinationHost;
				int _rtcStatsDestinationPort;
			};

			// There is one of those per call to CreateOffer().
			class CreateSdpObserver : public webrtc::CreateSessionDescriptionObserver {
			public:
				CreateSdpObserver(Concurrency::task_completion_event
					<webrtc::SessionDescriptionInterface*> tce);

				// CreateSessionDescriptionObserver implementation
				virtual void OnSuccess(webrtc::SessionDescriptionInterface* desc);
				virtual void OnFailure(const std::string& error);

			private:
				Concurrency::task_completion_event<webrtc::SessionDescriptionInterface*> _tce;
			};

			// There is one of those per call to CreateOffer().
			class SetSdpObserver : public webrtc::SetSessionDescriptionObserver {
			public:
				explicit SetSdpObserver(Concurrency::task_completion_event<void> tce);

				// SetSessionDescriptionObserver implementation
				virtual void OnSuccess();
				virtual void OnFailure(const std::string& error);

			private:
				Concurrency::task_completion_event<void> _tce;
			};

			// There is one of those per call to CreateDataChannel().
			class DataChannelObserver : public webrtc::DataChannelObserver {
			public:
				explicit DataChannelObserver(Org::WebRtc::RTCDataChannel^ channel);

				// DataChannelObserver implementation
				virtual void OnStateChange();
				virtual void OnMessage(const webrtc::DataBuffer& buffer);

			private:
				Org::WebRtc::RTCDataChannel^ _channel;
			};
		}
	}
}  // namespace Org::WebRtc::Internal

#endif  // WEBRTC_BUILD_WINRT_GYP_API_GLOBALOBSERVER_H_

