
// Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.


#ifndef WEBRTC_BUILD_WINRT_GYP_API_PEERCONNECTIONINTERFACE_H_
#define WEBRTC_BUILD_WINRT_GYP_API_PEERCONNECTIONINTERFACE_H_

#include <collection.h>
#include <vector>
#include "webrtc/api/peerconnectioninterface.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/scopedptrcollection.h"
#include "webrtc/base/logging.h"
#include "GlobalObserver.h"
#include "DataChannel.h"
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"
#include "webrtc/build/WinRT_gyp/Api/RTCStatsReport.h"

using Platform::String;
using Platform::IBox;
using Windows::Foundation::Collections::IVector;
using Windows::Foundation::IAsyncOperation;
using Windows::Foundation::IAsyncAction;
using Org::WebRtc::Internal::CreateSdpObserver;
using Org::WebRtc::Internal::SetSdpObserver;
using Org::WebRtc::Internal::DataChannelObserver;
using Org::WebRtc::Internal::GlobalObserver;

namespace Org {
	namespace WebRtc {

		ref class MediaStream;
		ref class MediaStreamTrack;

		public enum class LogLevel {
			LOGLVL_SENSITIVE = rtc::LS_SENSITIVE,
			LOGLVL_VERBOSE = rtc::LS_VERBOSE,
			LOGLVL_INFO = rtc::LS_INFO,
			LOGLVL_WARNING = rtc::LS_WARNING,
			LOGLVL_ERROR = rtc::LS_ERROR
		};
		/// <summary>
		/// Defines the paremeters of a media codec.
		/// </summary>
		public ref class CodecInfo sealed {
		private:
			int _id;
			int _clockrate;
			int _channels;
			String^ _name;
		public:
			CodecInfo(int id, int clockrate, String^ name) {
				_id = id;
				_clockrate = clockrate;
				_name = name;
			}
			/// <summary>
			/// Get or sets a unique identifier that represents a codec.
			/// </summary>
			property int Id {
				int get() {
					return _id;
				}
				void set(int value) {
					_id = value;
				}
			}
			/// <summary>
			/// Get or sets a clock rate in cycles per second.
			/// </summary>
			property int Clockrate {
				int get() {
					return _clockrate;
				}
				void set(int value) {
					_clockrate = value;
				}
			}
			/// <summary>
			/// Get or sets a display name that represents the codec.
			/// </summary>
			property String^ Name {
				String^ get() {
					return _name;
				}
				void set(String^ value) {
					_name = value;
				}
			}
		};

		[Windows::Foundation::Metadata::WebHostHidden]
		/// <summary>
		/// Defines static methods for handling generic WebRTC operations, for example
		/// controlling whether WebRTC tracing is enabled.
		/// </summary>
		public ref class WebRTC sealed {
		public:
			/// <summary>
			/// Gets permission from the OS to get access to a media capture device. If
			/// permissions are not enabled for the calling application, the OS will
			/// display a prompt asking the user for permission.
			/// This function must be called from the UI thread.
			/// </summary>
			static IAsyncOperation<bool>^ RequestAccessForMediaCapture();
			/// <summary>
			/// Initializes WebRTC dispatch and worker threads.
			/// </summary>
			static void Initialize(Windows::UI::Core::CoreDispatcher^ dispatcher);

			/// <summary>
			/// Check if WebRTC tracing is currently enabled.
			/// </summary>
			/// <returns>True if WebRTC tracing is currently enabled, false otherwise.</returns>
			static bool IsTracing();

			/// <summary>
			/// Starts WebRTC tracing.
			/// </summary>
			static void StartTracing();

			/// <summary>
			/// Stops WebRTC tracing.
			/// </summary>
			static void StopTracing();

			/// <summary>
			/// Saves the collected WebRTC trace information to a file.
			/// </summary>
			/// <param name="filename">Path of the file to save trace information to.</param>
			/// <returns>True if trace information was saved successfully, false otherwise.</returns>
			static bool SaveTrace(Platform::String^ filename);

			/// <summary>
			/// Can be used for sending the trace information from a device to a TCP client application
			/// running on a remote machine.
			/// </summary>
			/// <param name="host">The IP or name of the machine the trace information is collected on.</param>
			/// <param name="port">The port of the machine the trace information is collected on.</param>
			/// <returns>True if the socket to send trace information is created successfully,
			/// false otherwise.</returns>
			static bool SaveTrace(Platform::String^ host, int port);

			/// <summary>
			/// Starts WebRTC logging.
			/// </summary>
			/// <param name="level">Desired verbosity level for logging.</param>
			static void EnableLogging(LogLevel level);

			/// <summary>
			/// Stops WebRTC logging.
			/// </summary>
			static void DisableLogging();

			/// <summary>
			/// The folder where the app is currently saving the logging information.
			/// </summary>
			static property Windows::Storage::StorageFolder^ LogFolder {
				Windows::Storage::StorageFolder^ get();
			}

			/// <summary>
			/// The name of the file where the app is currently saving the logging information.
			/// </summary>
			static property String^ LogFileName {
				String^ get();
			}

			/// <summary>
			/// Retrieves the audio codecs supported by the device.
			/// </summary>
			/// <returns>A vector of supported audio codecs.</returns>
			static IVector<CodecInfo^>^ GetAudioCodecs();

			/// <summary>
			/// Retrieves the video codecs supported by the device.
			/// </summary>
			/// <returns>A vector of supported video codecs.</returns>
			static IVector<CodecInfo^>^ GetVideoCodecs();

			/// <summary>
			/// This method can be used to overwrite the preferred camera capabilities.
			/// </summary>
			/// <param name="frameWidth">Image width.</param>
			/// <param name="frameHeight">Image height.</param>
			/// <param name="fps">Frames per second.</param>
			static void SetPreferredVideoCaptureFormat(int frameWidth,
				int frameHeight,
				int fps);

			/// <summary>
			/// Synchronization with NTP is needed for end to end delay measurements,
			/// which involve multiple devices.
			/// </summary>
			/// <param name="currentNtpTime">NTP time in miliseconds.</param>
			static void SynNTPTime(int64 currentNtpTime);

			/// <summary>
			/// CPU usage statistics data (in percents). Should be set by application.
			/// </summary>
			static property double CpuUsage { double get(); void set(double value); }

			/// <summary>
			/// Memory usage statistics data (in bytes). Should be set by application.
			/// </summary>
			static property INT64 MemoryUsage { INT64 get(); void set(INT64 value); }

		private:
			// This type is not meant to be created.
			WebRTC();

			static const unsigned char* GetCategoryGroupEnabled(const char*
				category_group);
			static void __cdecl AddTraceEvent(char phase,
				const unsigned char* category_group_enabled,
				const char* name,
				uint64 id,
				int num_args,
				const char** arg_names,
				const unsigned char* arg_types,
				const uint64* arg_values,
				unsigned char flags);
		};

		/// <summary>
		/// Wrapper class that allows calling methods in <see cref="WebRTC"/> from WinJS.
		/// </summary>
		public ref class WinJSHooks sealed {
		public:
			static void initialize();
			static IAsyncOperation<bool>^ requestAccessForMediaCapture();
			static bool IsTracing();
			static void StartTracing();
			static void StopTracing();
			static bool SaveTrace(Platform::String^ filename);
			static bool SaveTrace(Platform::String^ host, int port);
		};

		public enum class RTCBundlePolicy {
			Balanced,
			MaxBundle,
			MaxCompat,
		};

		public enum class RTCIceTransportPolicy {
			None,
			Relay,
			NoHost,
			All
		};

		public enum class RTCIceGatheringState {
			New,
			Gathering,
			Complete
		};

		public enum class RTCIceConnectionState {
			New,
			Checking,
			Connected,
			Completed,
			Failed,
			Disconnected,
			Closed
		};

		/// <summary>
		/// Describes the type of an SDP blob.
		/// </summary>
		public enum class RTCSdpType {
			Offer,
			Pranswer,
			Answer,
		};

		public enum class RTCSignalingState {
			Stable,
			HaveLocalOffer,
			HaveRemoteOffer,
			HaveLocalPranswer,
			HaveRemotePranswer,
			Closed
		};

		/// <summary>
		/// Stores the configuration parameters of an ICE server.
		/// </summary>
		public ref class RTCIceServer sealed {
		public:
			/// <summary>
			/// Gets or sets an ICE server network address as a URL.
			/// </summary>
			property String^ Url;
			/// <summary>
			/// Gets or set a user name to login to the ice server.
			/// </summary>
			property String^ Username;
			/// <summary>
			/// Gets or sets the credentials to login to the ice server.
			/// </summary>
			property String^ Credential;
		};

		/// <summary>
		/// Stores the ICE servers configuration.
		/// </summary>
		public ref class RTCConfiguration sealed {
		public:
			/// <summary>
			/// Gets or sets a list of ICE servers and their configuration parameters.
			/// </summary>
			property IVector<RTCIceServer^>^ IceServers;
			/// <summary>
			/// Gets or sets the transport policy of the ICE servers.
			/// </summary>
			property IBox<RTCIceTransportPolicy>^ IceTransportPolicy;
			/// <summary>
			/// Get or sets the ICE server transport connection policy.
			/// </summary>
			property IBox<RTCBundlePolicy>^ BundlePolicy;
		};

		/// <summary>
		/// Stores ICE candidate parameters.
		/// </summary>
		public ref class RTCIceCandidate sealed {
		public:
			RTCIceCandidate();
			RTCIceCandidate(String^ candidate, String^ sdpMid,
				uint16 sdpMLineIndex);
			/// <summary>
			/// Gets or sets the name of the ICE candidate.
			/// </summary>
			property String^ Candidate;
			/// <summary>
			/// Gets or sets the SDP media identifier.
			/// </summary>
			property String^ SdpMid;
			/// <summary>
			/// Sets or get the "m=" line used as the ICE candidate in the SDP.
			/// </summary>
			property uint16 SdpMLineIndex;
		};

		/// <summary>
		/// An SDP blob and an associated <see cref="RTCSdpType"/>.
		/// </summary>
		public ref class RTCSessionDescription sealed {
		public:
			RTCSessionDescription();
			RTCSessionDescription(RTCSdpType type, String^ sdp);
			/// <summary>
			/// Gets or sets the SDP type.
			/// </summary>
			property IBox<RTCSdpType>^ Type;
			/// <summary>
			/// Gets or sets the complete raw SDP.
			/// </summary>
			property String^ Sdp;
		};

		// Events and delegates
		// ------------------
		/// <summary>
		/// Stores ICE candidate parameters received by an event.
		/// </summary>
		public ref class RTCPeerConnectionIceEvent sealed {
		public:
			/// <summary>
			/// Gets or set ICE candidate parameters.
			/// </summary>
			property RTCIceCandidate^ Candidate;
		};

		/// <summary>
		/// Stores ICE peer connection state received by an event.
		/// </summary>
		public ref class RTCPeerConnectionIceStateChangeEvent sealed {
		public:
			/// <summary>
			/// Gets or sets the ICE peer connection state.
			/// </summary>
			property RTCIceConnectionState State;
		};

		/// <summary>
		/// Stores peer connection statistics.
		/// </summary>
		public ref class RTCPeerConnectionHealthStats sealed {
		public:
			/// <summary>
			/// Gets or sets the number of bytes received during the
			/// lifetime of a peer connection.
			/// </summary>
			property int64 ReceivedBytes;
			/// <summary>
			/// Gets or sets the receive bit rate in Kilobits per second.
			/// </summary>
			property int64 ReceivedKpbs;
			/// <summary>
			/// Gets or sets the number of bytes sent during the
			/// lifetime of a peer connection.
			/// </summary>
			property int64 SentBytes;
			/// <summary>
			/// Gets or sets the send bit rate in Kilobits per second.
			/// </summary>
			property int64 SentKbps;
			/// <summary>
			/// Gets or set the round-trip time.
			/// </summary>
			property int64 RTT;
			/// <summary>
			/// Stores a description of the ICE candidate connected to this peer.
			/// </summary>
			property String^ LocalCandidateType;
			/// <summary>
			/// Stores a description of the ICE candidate connected to a remote peer.
			/// </summary>
			property String^ RemoteCandidateType;
		};

		/// <summary>
		/// Stores peer connection statistics received by an event.
		/// </summary>
		public ref class RTCStatsReportsReadyEvent sealed {
		public:
			/// <summary>
			/// Gets or sets peer connection statistics.
			/// </summary>
			property RTCStatsReports rtcStatsReports;
		};

		// ------------------
		/// <summary>
		/// Stores media stream object received by an event.
		/// </summary>
		public ref class MediaStreamEvent sealed {
		public:
			/// <summary>
			/// Gets or sets a media stream object.
			/// </summary>
			property MediaStream^ Stream;
		};
		/// <summary>
		/// An RTCPeerConnection allows two users to communicate directly.
		/// Communications are coordinated via a signaling channel which is
		/// provided by unspecified means.
		/// </summary>
		/// <remarks>
		/// http://www.w3.org/TR/webrtc/#peer-to-peer-connections
		///  </remarks>
		public ref class RTCPeerConnection sealed {
		public:
			// Required so the observer can raise events in this class.
			// By default event raising is protected.
			friend class GlobalObserver;

			/// <summary>
			/// Creates an RTCPeerConnection object.
			/// </summary>
			/// <remarks>
			/// Refer to http://www.w3.org/TR/webrtc for the RTCPeerConnection
			/// construction algorithm
			/// </remarks>
			/// <param name="configuration">
			/// The configuration has the information to find and access the
			/// servers used by ICE.
			/// </param>
			RTCPeerConnection(RTCConfiguration^ configuration);

			/// <summary>
			/// A new ICE candidate has been found.
			/// </summary>
			event RTCPeerConnectionIceEventDelegate^ OnIceCandidate;

			/// <summary>
			/// A state transition has occurred for the <see cref="IceConnectionState"/>.
			/// </summary>
			event RTCPeerConnectionIceStateChangeEventDelegate^ OnIceConnectionChange;

			/// <summary>
			/// The remote peer has added a new <see cref="MediaStream"/> to this
			/// connection.
			/// </summary>
			event MediaStreamEventEventDelegate^ OnAddStream;

			/// <summary>
			/// The remote peer removed a <see cref="MediaStream"/>.
			/// </summary>
			event MediaStreamEventEventDelegate^ OnRemoveStream;

			/// <summary>
			/// Session (re-)negotiation is needed.
			/// </summary>
			event EventDelegate^ OnNegotiationNeeded;

			/// <summary>
			/// A state transition has occurred for the <see cref="SignalingState"/>.
			/// </summary>
			event EventDelegate^ OnSignalingStateChange;

			/// <summary>
			/// A remote peer has opened a data channel.
			/// </summary>
			event RTCDataChannelEventDelegate^ OnDataChannel;

			/// <summary>
			/// New connection health stats are available.
			/// </summary>
			event RTCPeerConnectionHealthStatsDelegate^ OnConnectionHealthStats;

			/// <summary>
			/// Webrtc statistics report is ready <see cref="RTCStatsReports"/>.
			/// </summary>
			event RTCStatsReportsReadyEventDelegate^ OnRTCStatsReportsReady;


			/// <summary>
			/// Generates a blob of SDP that contains an RFC 3264 offer with the
			///  supported configurations for the session, including descriptions of
			/// the local MediaStreams attached to this <see cref="RTCPeerConnection"/>,
			/// the codec/RTP/RTCP options supported by this implementation, and any
			/// candidates that have been gathered by the ICE Agent.
			/// </summary>
			/// <returns></returns>
			IAsyncOperation<RTCSessionDescription^>^ CreateOffer();

			/// <summary>
			/// Generates an SDP answer with the supported configuration for the
			/// session that is compatible with the parameters in the remote
			/// configuration. Like createOffer, the returned blob contains descriptions
			/// of the local MediaStreams attached to this
			/// <see cref="RTCPeerConnection"/>, the codec/RTP/RTCP options negotiated
			/// for this session, and any candidates that have been gathered by the ICE
			/// Agent.
			/// </summary>
			/// <returns>An action which completes asynchronously</returns>
			IAsyncOperation<RTCSessionDescription^>^ CreateAnswer();

			/// <summary>
			/// Instructs the <see cref="RTCPeerConnection"/> to apply the supplied
			/// <see cref="RTCSessionDescription"/> as the local description.
			/// This API changes the local media state.
			/// </summary>
			/// <param name="description">RTCSessionDescription to apply as the local
			/// description</param>
			/// <returns>An action which completes asynchronously</returns>
			IAsyncAction^ SetLocalDescription(RTCSessionDescription^ description);

			/// <summary>
			/// Instructs the <see cref="RTCPeerConnection"/> to apply the supplied
			/// <see cref="RTCSessionDescription"/> as the remote offer or answer.
			/// This API changes the local media state.
			/// </summary>
			/// <param name="description"><see cref="RTCSessionDescription"/> to
			/// apply as the local description</param>
			/// <returns>An action which completes asynchronously</returns>
			IAsyncAction^ SetRemoteDescription(RTCSessionDescription^ description);

			/// <summary>
			/// Gets the configuration of this connection.
			/// </summary>
			/// <returns>A handle to the current configuration for this object.</returns>
			RTCConfiguration^ GetConfiguration();

			/// <summary>
			/// Returns an <see cref="IVector"/> that represents a snapshot of all the
			/// <see cref="MediaStream"/>
			/// that this <see cref="RTCPeerConnection"/> is currently sending.
			/// </summary>
			/// <returns>A sequence of handles to the <see cref="MediaStream"/>
			/// objects representing the streams that are currently being sent
			/// with this RTCPeerConnection object.</returns>
			IVector<MediaStream^>^ GetLocalStreams();

			/// <summary>
			/// Returns an <see cref="IVector"/> that represents a snapshot of all the
			/// <see cref="MediaStream"/> that this <see cref="RTCPeerConnection"/> is
			/// currently receiving.
			/// </summary>
			/// <returns>A sequence of handles to the <see cref="MediaStream"/> objects
			/// representing the streams that are currently being received by this
			/// RTCPeerConnection object.</returns>
			IVector<MediaStream^>^ GetRemoteStreams();

			/// <summary>
			/// If this object is currently sending or receiving a
			/// <see cref="MediaStream"/> with the provided
			/// <paramref name="streamId"/>, a handle to that stream is returned.
			/// </summary>
			/// <param name="streamId">Identifier of the stream being requested.</param>
			/// <returns>A handle to the local or remote <see cref="MediaStream"/> with
			/// the given <paramref name="streamId"/> if one exists, nullptr if no stream
			/// is found.</returns>
			MediaStream^ GetStreamById(String^ streamId);

			/// <summary>
			/// Adds a new local <see cref="MediaStream"/> to be sent on this connection.
			/// </summary>
			/// <param name="stream"><see cref="MediaStream"/> to be added.</param>
			void AddStream(MediaStream^ stream);

			/// <summary>
			/// Removes a local <see cref="MediaStream"/> from this connection.
			/// </summary>
			/// <param name="stream"><see cref="MediaStream"/> to be removed.</param>
			void RemoveStream(MediaStream^ stream);

			/// <summary>
			/// Creates a new <see cref="RTCDataChannel"/> object with the given
			/// <paramref name="label"/>.
			/// </summary>
			/// <param name="label">Used as the descriptive name for the new data
			/// channel.</param>
			/// <param name="init">Can be used to configure properties of the underlying
			/// channel such as data reliability.</param>
			/// <returns>The newly created <see cref="RTCDataChannel"/>.</returns>
			RTCDataChannel^ CreateDataChannel(String^ label, RTCDataChannelInit^ init);

			/// <summary>
			/// Provides a remote candidate to the ICE Agent.
			/// The candidate is added to the remote description.
			/// This call will result in a change to the connection state of the ICE
			/// Agent, and may lead to a change to media state if it results in different
			/// connectivity being established.
			/// </summary>
			/// <param name="candidate">candidate to be added to the remote description
			/// </param>
			/// <returns>An action which completes asynchronously</returns>
			IAsyncAction^ AddIceCandidate(RTCIceCandidate^ candidate);

			/// <summary>
			/// Ends any active ICE processing or streaming, releases resources.
			/// </summary>
			void Close();

			/// <summary>
			/// Enable/Disable WebRTC statistics to ETW.
			/// </summary>;
			property bool EtwStatsEnabled { bool get(); void set(bool value); }

			/// <summary>
			/// Enable/Disable connection health statistics.
			/// When new connection health stats are available OnConnectionHealthStats
			///  event is raised.
			/// </summary>
			property bool ConnectionHealthStatsEnabled { bool get(); void set(bool value); }

			/// <summary>
			/// Enable/Disable WebRTC statistics for exposing report.
			/// </summary>
			property bool RtcStatsEnabled { bool get(); void set(bool value); }

			/// <summary>
			/// Enable/Disable send of WebRTC statistics in JSON format to a TCP server.
			/// Destination can be configured using <see cref="RtcStatsDestinationHost"/>
			/// and <see cref="RtcStatsDestinationPort"/>.
			/// </summary>
			property bool SendRtcStatsToRemoteHostEnabled { bool get(); void set(bool value); }

			/// <summary>
			/// Hostname of the machine to send WebRTC statistics to.
			/// The machine should accept TCP connections at port configurable using 
			/// <see cref="RtcStatsDestinationPort"/> property.
			/// Default value: localhost
			/// </summary>
			property String^ RtcStatsDestinationHost { String^ get(); void set(String^ value); }

			/// <summary>
			/// Port of the machine to send WebRTC statistics to.
			/// Default value: 47005
			/// </summary>
			property int RtcStatsDestinationPort { int get(); void set(int value); }

			/// <summary>
			/// The last <see cref="RTCSessionDescription"/> that was successfully set
			/// using <see cref="SetLocalDescription"/>, plus any local candidates that
			/// have been generated by the ICE Agent since then.
			/// A nullptr handle will be returned if the local description has not yet
			/// been set.
			/// </summary>
			property RTCSessionDescription^ LocalDescription {
				RTCSessionDescription^ get();
			}

			/// <summary>
			/// The last <see cref="RTCSessionDescription"/> that was successfully set
			/// using <see cref="SetRemoteDescription"/>,
			/// plus any remote candidates that have been supplied via
			/// <see cref="AddIceCandidate"/> since then.
			/// A nullptr handle will be returned if the local description has not yet
			/// been set.
			/// </summary>
			property RTCSessionDescription^ RemoteDescription {
				RTCSessionDescription^ get();
			}

			/// <summary>
			/// Keeps track of the current signaling state. State transitions may be
			///  triggered when alocal or remote offer is applied, when a local or remote
			// answer or pranswer is applied, or when the connection is closed.
			/// </summary>
			property RTCSignalingState SignalingState {
				RTCSignalingState get();
			}

			/// <summary>
			/// Gets the ICE gathering state such as New, Gathering, or Complete.
			/// </summary>
			property RTCIceGatheringState IceGatheringState {
				RTCIceGatheringState get();
			}

			/// <summary>
			/// Gets the state of the connection.
			/// </summary>
			property RTCIceConnectionState IceConnectionState {
				RTCIceConnectionState get();
			}

		private:
			~RTCPeerConnection();
			rtc::scoped_refptr<webrtc::PeerConnectionInterface> _impl;
			// This lock protects _impl.
			rtc::scoped_ptr<webrtc::CriticalSectionWrapper> _lock;

			rtc::scoped_ptr<GlobalObserver> _observer;

			typedef std::vector<rtc::scoped_refptr<CreateSdpObserver>> CreateSdpObservers;
			typedef std::vector<rtc::scoped_refptr<SetSdpObserver>> SetSdpObservers;
			typedef rtc::ScopedPtrCollection<DataChannelObserver> DataChannelObservers;
			CreateSdpObservers _createSdpObservers;
			SetSdpObservers _setSdpObservers;
			DataChannelObservers _dataChannelObservers;
		};

		namespace globals {
			extern rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>
				gPeerConnectionFactory;

			/// <summary>
			/// The worker thread for webrtc.
			/// </summary>
			extern rtc::Thread gThread;

			template <typename T>
			T RunOnGlobalThread(std::function<T()> fn) {
				return gThread.Invoke<T, std::function<T()>>(fn);
			}

		}  // namespace globals
	}
}  // namespace Org.WebRtc

#endif  // WEBRTC_BUILD_WINRT_GYP_API_PEERCONNECTIONINTERFACE_H_
