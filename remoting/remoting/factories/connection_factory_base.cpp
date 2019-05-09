
#include "connection_factory_base.h"

using namespace std::placeholders;

hololight::remoting::factories::connection_factory_base::connection_factory_base(std::unique_ptr<hololight::signaling::webrtc::WebrtcSignalingRelay> relay) :
	_relay(std::move(relay))
{
	_relay->register_sdp_handler(std::bind(&connection_factory_base::handle_remote_sdp, this, _1));
	_relay->register_ice_candidate_handler(std::bind(&connection_factory_base::handle_remote_ice_cand, this, _1));
}

void hololight::remoting::factories::connection_factory_base::handle_remote_sdp(hololight::signaling::webrtc::Sdp & sdp)
{
	set_remote_sdp(sdp);
}

void hololight::remoting::factories::connection_factory_base::handle_remote_ice_cand(hololight::signaling::webrtc::IceCandidate & cand)
{
	set_remote_ice_cand(cand);
}

void hololight::remoting::factories::connection_factory_base::send_sdp(hololight::signaling::webrtc::Sdp & sdp)
{
	_relay->send_sdp_async(sdp);
}

void hololight::remoting::factories::connection_factory_base::send_ice_candidate(hololight::signaling::webrtc::IceCandidate & cand)
{
	_relay->send_ice_candidate_async(cand);
}
