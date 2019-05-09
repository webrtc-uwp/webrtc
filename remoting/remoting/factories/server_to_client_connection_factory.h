#ifndef HOLOLIGHT_REMOTING_FACTORIES_SERVER_TO_CLIENT_FACTORY_H
#define HOLOLIGHT_REMOTING_FACTORIES_SERVER_TO_CLIENT_FACTORY_H

#include "connection_factory_base.h"
#include "../server_to_client_connection.h"
#include "../../signaling/webrtc/webrtc_signaling_relay.h"
#include <d3d11.h>
#include <future>

namespace hololight::remoting::factories {
	class server_to_client_connection_factory : public connection_factory_base
	{
	public:
		server_to_client_connection_factory() = delete;
		server_to_client_connection_factory(std::unique_ptr<hololight::signaling::webrtc::WebrtcSignalingRelay> relay);
		std::unique_ptr<hololight::remoting::server_to_client_connection> create_connection(ID3D11Device* device, ID3D11DeviceContext* deviceContext, D3D11_TEXTURE2D_DESC renderTargetDesc);

	private:

		void create_peerconnection(ID3D11Device* device, ID3D11DeviceContext* deviceContext, D3D11_TEXTURE2D_DESC renderTargetDesc);
		void add_video_stream();
		void add_datachannel();

		hololight::signaling::webrtc::Sdp create_and_set_offer();

		void wait_until_connection_ready();

		// connection_factory_base implementation
		void set_remote_sdp(hololight::signaling::webrtc::Sdp& sdp);
		void set_remote_ice_cand(hololight::signaling::webrtc::IceCandidate& cand);

		// implementation details using old C API. TODO: Replace this once this code gets integrated into the webrtc plugin codebase

		int _peerConnectionId = -1;
		std::promise<hololight::signaling::webrtc::Sdp> _offerPromise;
		std::promise<void> _localFrameReady;

		void register_callbacks();
		void unregister_callbacks();

		static void OnLocalSDPReady(const char* type, const char* sdp, void* userData);

		static void OnIceCandidateReadyToSend(const char* candidate,
			const int sdp_mline_index,
			const char* sdp_mid,
			void* userData);
	};
}

#endif // HOLOLIGHT_REMOTING_FACTORIES_SERVER_TO_CLIENT_FACTORY_H