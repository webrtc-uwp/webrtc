#ifndef HOLOLIGHT_REMOTING_FACTORIES_CONNECTION_FACTORY_BASE_H
#define HOLOLIGHT_REMOTING_FACTORIES_CONNECTION_FACTORY_BASE_H

#include "../../signaling/webrtc/webrtc_signaling_relay.h"

namespace hololight::remoting::factories {
	class connection_factory_base
	{
	protected:
		connection_factory_base(std::unique_ptr<hololight::signaling::webrtc::WebrtcSignalingRelay> relay);

		void send_sdp(hololight::signaling::webrtc::Sdp & sdp);
		void send_ice_candidate(hololight::signaling::webrtc::IceCandidate & cand);

		virtual void set_remote_sdp(hololight::signaling::webrtc::Sdp & sdp) = 0;
		virtual void set_remote_ice_cand(hololight::signaling::webrtc::IceCandidate & cand) = 0;

		std::unique_ptr<hololight::signaling::webrtc::WebrtcSignalingRelay> _relay;

	private:
		void handle_remote_sdp(hololight::signaling::webrtc::Sdp & sdp);
		void handle_remote_ice_cand(hololight::signaling::webrtc::IceCandidate & cand);
	};
}

#endif // HOLOLIGHT_REMOTING_FACTORIES_CONNECTION_FACTORY_BASE_H