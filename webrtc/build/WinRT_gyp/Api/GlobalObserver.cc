
// Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

// Class1.cpp
#include <ppltasks.h>
#include <vector>

#include "webrtc/build/WinRT_gyp/Api/GlobalObserver.h"
#include "PeerConnectionInterface.h"
#include "Marshalling.h"
#include "Media.h"
#include "DataChannel.h"
#include "webrtc/common_video/video_common_winrt.h"

using Platform::Collections::Vector;

namespace Org {
	namespace WebRtc {
		namespace Internal {

#define POST_PC_EVENT(fn, evt) \
  auto pc = _pc;\
  Windows::UI::Core::CoreDispatcher^ _windowDispatcher = webrtc::VideoCommonWinRT::GetCoreDispatcher();\
  if (_windowDispatcher != nullptr) {\
    _windowDispatcher->RunAsync(\
      Windows::UI::Core::CoreDispatcherPriority::Normal, \
      ref new Windows::UI::Core::DispatchedHandler([pc, evt] {\
      if (pc != nullptr) {\
        pc->##fn(evt);\
      }\
    }));\
  } else {\
    if (pc != nullptr) {\
      pc->##fn(evt);\
    }\
  }

#define POST_PC_ACTION(fn) \
  auto pc = _pc;\
  Windows::UI::Core::CoreDispatcher^ _windowDispatcher = webrtc::VideoCommonWinRT::GetCoreDispatcher();\
  if (_windowDispatcher != nullptr) {\
    _windowDispatcher->RunAsync(\
      Windows::UI::Core::CoreDispatcherPriority::Normal, \
      ref new Windows::UI::Core::DispatchedHandler([pc] {\
      if (pc != nullptr) {\
        pc->##fn();\
      }\
    }));\
  } else {\
    if (pc != nullptr) {\
      pc->##fn();\
    }\
  }

			GlobalObserver::GlobalObserver() {
				ResetStatsConfig();
			}

			void GlobalObserver::SetPeerConnection(
				Org::WebRtc::RTCPeerConnection^ pc) {
				_pc = pc;
				if (_pc == nullptr) {
					_stats_observer = nullptr;
				}
				ResetStatsConfig();
			}

			void GlobalObserver::ResetStatsConfig() {
				_etwStatsEnabled = false;
				_connectionHealthStatsEnabled = false;
				_rtcStatsEnabled = false;
				_sendRtcStatsToRemoteHostEnabled = false;
				_rtcStatsDestinationHost = "localhost";
				_rtcStatsDestinationPort = 47005;
			}

			void GlobalObserver::EnableETWStats(bool enable) {
				_etwStatsEnabled = enable;
				if (_stats_observer) {
					_stats_observer->ToggleETWStats(enable);
				}
			}

			bool GlobalObserver::AreETWStatsEnabled() {
				return _etwStatsEnabled;
			}

			void GlobalObserver::EnableConnectionHealthStats(bool enable) {
				_connectionHealthStatsEnabled = enable;
				if (_stats_observer) {
					_stats_observer->ToggleConnectionHealthStats(enable ? this : NULL);
				}
			}

			bool GlobalObserver::AreConnectionHealthStatsEnabled() {
				return _connectionHealthStatsEnabled;
			}

			void GlobalObserver::EnableRTCStats(bool enable) {
				_rtcStatsEnabled = enable;
				if (_stats_observer) {
					_stats_observer->ToggleRTCStats(enable ? this : NULL);
				}
			}

			bool GlobalObserver::AreRTCStatsEnabled() {
				return _rtcStatsEnabled;
			}

			void GlobalObserver::EnableSendRtcStatsToRemoteHost(bool enable) {
				_sendRtcStatsToRemoteHostEnabled = enable;
				if (_stats_observer) {
					if (enable) {
						_stats_observer->SetStatsNetworkDestination(
							_rtcStatsDestinationHost, _rtcStatsDestinationPort);
					}
					_stats_observer->ToggleStatsSendToRemoteHost(enable);
				}
			}

			bool GlobalObserver::IsSendRtcStatsToRemoteHostEnabled() {
				return _sendRtcStatsToRemoteHostEnabled;
			}

			void GlobalObserver::SetRtcStatsDestinationHost(std::string value) {
				_rtcStatsDestinationHost = value;
			}

			std::string GlobalObserver::GetRtcStatsDestinationHost() {
				return _rtcStatsDestinationHost;
			}

			void GlobalObserver::SetRtcStatsDestinationPort(int port) {
				_rtcStatsDestinationPort = port;
			}
			int GlobalObserver::GetRtcStatsDestinationPort() {
				return _rtcStatsDestinationPort;
			}


			// Triggered when the SignalingState changed.
			void GlobalObserver::OnSignalingChange(
				webrtc::PeerConnectionInterface::SignalingState new_state) {
				POST_PC_ACTION(OnSignalingStateChange);
			}

			// Triggered when SignalingState or IceState have changed.
			// TODO(bemasc): Remove once callers transition to OnSignalingChange.
			void GlobalObserver::OnStateChange(StateType state_changed) {
			}

			// Triggered when media is received on a new stream from remote peer.
			void GlobalObserver::OnAddStream(webrtc::MediaStreamInterface* stream) {
				auto evt = ref new Org::WebRtc::MediaStreamEvent();
				evt->Stream = ref new Org::WebRtc::MediaStream(stream);
				POST_PC_EVENT(OnAddStream, evt);
			}

			// Triggered when a remote peer close a stream.
			void GlobalObserver::OnRemoveStream(webrtc::MediaStreamInterface* stream) {
				auto evt = ref new Org::WebRtc::MediaStreamEvent();
				evt->Stream = ref new Org::WebRtc::MediaStream(stream);
				POST_PC_EVENT(OnRemoveStream, evt);
			}

			// Triggered when a remote peer open a data channel.
			void GlobalObserver::OnDataChannel(webrtc::DataChannelInterface* data_channel) {
				auto evt = ref new Org::WebRtc::RTCDataChannelEvent();
				evt->Channel = ref new Org::WebRtc::RTCDataChannel(data_channel);
				// This observer is deleted when the channel closes.
				// See DataChannelObserver::OnStateChange().
				data_channel->RegisterObserver(new DataChannelObserver(evt->Channel));
				POST_PC_EVENT(OnDataChannel, evt);
			}

			// Triggered when renegotiation is needed, for example the ICE has restarted.
			void GlobalObserver::OnRenegotiationNeeded() {
				POST_PC_ACTION(OnNegotiationNeeded);
			}

			// Called any time the IceConnectionState changes
			void GlobalObserver::OnIceConnectionChange(
				webrtc::PeerConnectionInterface::IceConnectionState new_state) {
				if (new_state == webrtc::PeerConnectionInterface::kIceConnectionConnected) {
					if (!_stats_observer.get()) {
						_stats_observer =
							new rtc::RefCountedObject<webrtc::WebRTCStatsObserver>(_pc->_impl);
					}
					_stats_observer->ToggleETWStats(_etwStatsEnabled);
					_stats_observer->ToggleConnectionHealthStats(
						_connectionHealthStatsEnabled ? this : NULL);
					_stats_observer->ToggleRTCStats(
						_rtcStatsEnabled ? this : NULL);
					if (_sendRtcStatsToRemoteHostEnabled) {
						_stats_observer->SetStatsNetworkDestination(
							_rtcStatsDestinationHost, _rtcStatsDestinationPort);
					}
					_stats_observer->ToggleStatsSendToRemoteHost(_sendRtcStatsToRemoteHostEnabled);
				}
				auto evt = ref new Org::WebRtc::RTCPeerConnectionIceStateChangeEvent();
				Org::WebRtc::RTCIceConnectionState cxNewState;
				ToCx(new_state, &cxNewState);
				evt->State = cxNewState;
				POST_PC_EVENT(OnIceConnectionChange, evt);
			}

			// Called any time the IceGatheringState changes
			void GlobalObserver::OnIceGatheringChange(
				webrtc::PeerConnectionInterface::IceGatheringState new_state) {
				LOG(LS_INFO) << "OnIceGatheringChange";
			}

			// New Ice candidate have been found.
			void GlobalObserver::OnIceCandidate(
				const webrtc::IceCandidateInterface* candidate) {
				std::string c;
				candidate->ToString(&c);
				LOG(LS_INFO) << "Ice candidate = " << c;
				auto evt = ref new Org::WebRtc::RTCPeerConnectionIceEvent();
				Org::WebRtc::RTCIceCandidate^ cxCandidate;
				if (candidate == nullptr) {
					evt->Candidate = nullptr;
				}
				else {
					ToCx(*candidate, &cxCandidate);
					evt->Candidate = cxCandidate;
				}
				POST_PC_EVENT(OnIceCandidate, evt);
			}

			// TODO(bemasc): Remove this once callers transition to OnIceGatheringChange.
			// All Ice candidates have been found.
			void GlobalObserver::OnIceComplete() {
				auto evt = ref new Org::WebRtc::RTCPeerConnectionIceEvent();
				evt->Candidate = nullptr;
				POST_PC_EVENT(OnIceCandidate, evt);
			}

			void GlobalObserver::OnConnectionHealthStats(
				const webrtc::ConnectionHealthStats& stats) {
				auto evt = ref new Org::WebRtc::RTCPeerConnectionHealthStats();
				evt->ReceivedBytes = stats.received_bytes;
				evt->ReceivedKpbs = stats.received_kbps;
				evt->SentBytes = stats.sent_bytes;
				evt->SentKbps = stats.sent_kbps;
				evt->RTT = stats.rtt;
				evt->LocalCandidateType = ToCx(stats.local_candidate_type);
				evt->RemoteCandidateType = ToCx(stats.remote_candidate_type);
				POST_PC_EVENT(OnConnectionHealthStats, evt);
			}

			void GlobalObserver::OnRTCStatsReportsReady(
				const Org::WebRtc::RTCStatsReports& rtcStatsReports) {
				auto evt = ref new Org::WebRtc::RTCStatsReportsReadyEvent();
				evt->rtcStatsReports = rtcStatsReports;
				POST_PC_EVENT(OnRTCStatsReportsReady, evt);
			}

			//============================================================================

			CreateSdpObserver::CreateSdpObserver(
				Concurrency::task_completion_event<webrtc::SessionDescriptionInterface*> tce)
				: _tce(tce) {
			}

			void CreateSdpObserver::OnSuccess(webrtc::SessionDescriptionInterface* desc) {
				_tce.set(desc);
			}

			void CreateSdpObserver::OnFailure(const std::string& error) {
				_tce.set_exception(error);
			}

			//============================================================================

			SetSdpObserver::SetSdpObserver(Concurrency::task_completion_event<void> tce)
				: _tce(tce) {
			}

			void SetSdpObserver::OnSuccess() {
				_tce.set();
			}

			void SetSdpObserver::OnFailure(const std::string& error) {
				_tce.set_exception(error);
			}

			//============================================================================

			DataChannelObserver::DataChannelObserver(
				Org::WebRtc::RTCDataChannel^ channel)
				: _channel(channel) {
			}

			void DataChannelObserver::OnStateChange() {
				Windows::UI::Core::CoreDispatcher^ _windowDispatcher = webrtc::VideoCommonWinRT::GetCoreDispatcher();
				switch (_channel->GetImpl()->state()) {
				case webrtc::DataChannelInterface::kOpen:
					if (_windowDispatcher != nullptr) {
						_windowDispatcher->RunAsync(
							Windows::UI::Core::CoreDispatcherPriority::Normal,
							ref new Windows::UI::Core::DispatchedHandler([this] {
							_channel->OnOpen();
						}));
					}
					else {
						_channel->OnOpen();
					}
					break;
				case webrtc::DataChannelInterface::kClosed:
					_channel->_impl->UnregisterObserver();
					if (_windowDispatcher != nullptr) {
						_windowDispatcher->RunAsync(
							Windows::UI::Core::CoreDispatcherPriority::Normal,
							ref new Windows::UI::Core::DispatchedHandler([this] {
							_channel->OnClose();
							delete this;
						}));
					}
					else {
						_channel->OnClose();
						delete this;
					}
					break;
				}
			}

			void DataChannelObserver::OnMessage(const webrtc::DataBuffer& buffer) {
				auto evt = ref new Org::WebRtc::RTCDataChannelMessageEvent();

				if (!buffer.binary) {
					// convert buffer data from uint_8[] to char*
					String^ receivedString = ToCx(std::string(
						reinterpret_cast<const char*>(buffer.data.data()),
						buffer.size()));

					evt->Data = ref new Org::WebRtc::StringDataChannelMessage(
						receivedString);
				}
				else {
					// convert byte[] from buffer to Vector
					std::vector<byte> bytesFromBuffer = std::vector<byte>();
					bytesFromBuffer.insert(bytesFromBuffer.end(), buffer.data.data(),
						buffer.data.data() + buffer.size());
					Vector<byte>^ convertedBytes = ref new Vector<byte>();
					ToCx(&bytesFromBuffer, convertedBytes);

					evt->Data = ref new Org::WebRtc::BinaryDataChannelMessage(
						convertedBytes);
				}


				Windows::UI::Core::CoreDispatcher^ _windowDispatcher = webrtc::VideoCommonWinRT::GetCoreDispatcher();
				if (_windowDispatcher != nullptr) {
					_windowDispatcher->RunAsync(
						Windows::UI::Core::CoreDispatcherPriority::Normal,
						ref new Windows::UI::Core::DispatchedHandler([this, evt] {
						_channel->OnMessage(evt);
					}));
				}
				else {
					_channel->OnMessage(evt);
				}
			}
		}
	}
}  // namespace Org.WebRtc.Internal

