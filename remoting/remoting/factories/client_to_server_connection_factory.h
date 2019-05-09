#ifndef HOLOLIGHT_REMOTING_FACTORIES_CLIENT_TO_SERVER_CONNECTION_FACTORY_H
#define HOLOLIGHT_REMOTING_FACTORIES_CLIENT_TO_SERVER_CONNECTION_FACTORY_H

#include "connection_factory_base.h"
#include "../../signaling/webrtc/webrtc_signaling_relay.h"
#include "../client_to_server_connection.h"
#include "../video/video_frame_buffer.h"

#include <future>

namespace hololight::remoting::factories {
	class client_to_server_connection_factory : public connection_factory_base
	{
	public:
		client_to_server_connection_factory() = delete;
		client_to_server_connection_factory(std::unique_ptr<hololight::signaling::webrtc::WebrtcSignalingRelay> relay);

		std::unique_ptr<hololight::remoting::client_to_server_connection> create_connection();

	private:
		void create_peerconnection();
		void wait_until_remote_offer_set();

		hololight::signaling::webrtc::Sdp create_and_set_answer();

		void wait_until_connection_ready();

		// connection_factory_base implementation
		void set_remote_sdp(hololight::signaling::webrtc::Sdp& sdp);
		void set_remote_ice_cand(hololight::signaling::webrtc::IceCandidate& cand);

		// Implementation specific code
		int _peerConnectionId = -1;

		void register_callbacks();
		void unregister_callbacks();

		std::promise<void> _offerSetPromise;
		std::promise<hololight::signaling::webrtc::Sdp> _answerPromise;
		std::promise<void> _remoteFrameReady;

		// We keep a buffer for storing the first frame we receive, then we hand it to the client_to_server_connection instance
		std::unique_ptr<hololight::remoting::video::video_frame_buffer> _videoFrameBuffer;

		static void OnLocalSDPReady(const char* type, const char* sdp, void* userData);

		// We use this to detect if the connection is established
		static void OnRemoteFrameReady(
			const uint8_t* data_y,
			const uint8_t* data_u,
			const uint8_t* data_v,
			const uint8_t* data_a,
			int stride_y,
			int stride_u,
			int stride_v,
			int stride_a,
			uint32_t width,
			uint32_t height,
			void* userData);

		bool _receivedFirstFrame = false;

		static void OnLocalICECandidate(const char* candidate,
			const int sdp_mline_index,
			const char* sdp_mid,
			void* userData);
	};
}

#endif // HOLOLIGHT_REMOTING_FACTORIES_CLIENT_TO_SERVER_CONNECTION_FACTORY_H