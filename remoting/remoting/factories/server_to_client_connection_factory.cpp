
#include "server_to_client_connection_factory.h"

#include "../../util/future_utils.h"
#include <chrono>
#include <cassert>
#include "rtc_base/logging.h"
#include "../../unity_plugin_apis.h"

using namespace hololight::signaling::webrtc;
using namespace std::chrono_literals;

namespace hololight::remoting::factories {
	server_to_client_connection_factory::server_to_client_connection_factory(std::unique_ptr<hololight::signaling::webrtc::WebrtcSignalingRelay> relay) :
		connection_factory_base(std::move(relay))
	{
	}

	std::unique_ptr<hololight::remoting::server_to_client_connection> server_to_client_connection_factory::create_connection(ID3D11Device* device, ID3D11DeviceContext* deviceContext, D3D11_TEXTURE2D_DESC renderTargetDesc)
	{
		create_peerconnection(device, deviceContext, renderTargetDesc);

		add_video_stream();
		add_datachannel();

		auto offer = create_and_set_offer();
		send_sdp(offer);

		wait_until_connection_ready();

		RTC_LOG(LS_INFO) << "S2C Connection established";

		return std::make_unique<hololight::remoting::server_to_client_connection>(_peerConnectionId);
	}

	///////////////// Stupid implementation using the C API. This should be replaced by Kris' nice Connection class

	void server_to_client_connection_factory::create_peerconnection(ID3D11Device* device, ID3D11DeviceContext* deviceContext, D3D11_TEXTURE2D_DESC renderTargetDesc)
	{
		_peerConnectionId = CreatePeerConnectionWithD3D(
			nullptr,
			0,
			nullptr,
			nullptr,
			false,
			device,
			deviceContext,
			renderTargetDesc);

		assert(0 <= _peerConnectionId); // Ideally, we would check this on every function call

		register_callbacks();
	}

	void server_to_client_connection_factory::register_callbacks() {

		RegisterOnIceCandiateReadytoSend(_peerConnectionId, &server_to_client_connection_factory::OnIceCandidateReadyToSend, this);
		RegisterOnLocalSdpReadytoSend(_peerConnectionId,
			&server_to_client_connection_factory::OnLocalSDPReady, this);

	}

	void server_to_client_connection_factory::unregister_callbacks() {

		RegisterOnIceCandiateReadytoSend(_peerConnectionId, nullptr, nullptr);
		RegisterOnLocalSdpReadytoSend(_peerConnectionId, nullptr, nullptr);

	}

	void server_to_client_connection_factory::add_video_stream()
	{
		// Nothing to do i think. Happens in plugin code NO IT DOESN'T, I'M RETARDED!
		AddStream(_peerConnectionId, false);
	}

	void server_to_client_connection_factory::add_datachannel()
	{
		AddDataChannel(_peerConnectionId);
	}

	hololight::signaling::webrtc::Sdp server_to_client_connection_factory::create_and_set_offer()
	{
		CreateOffer(_peerConnectionId);

		auto fut = _offerPromise.get_future();

		while (!hololight::util::future_utils::is_ready(fut))
		{
			_relay->poll_messages();

			std::this_thread::yield();
		}

		auto offer = fut.get();

		return offer;
	}

	void server_to_client_connection_factory::wait_until_connection_ready()
	{
		// This is kinda dumb. We don't really have a "ready" callback

		//auto fut = _localFrameReady.get_future();

		//while (!hololight::util::future_utils::is_ready(fut))
		//{
		//	_relay->poll_messages();

		//	std::this_thread::yield();
		//}

		//fut.wait();

		for (int i = 0; i < 300; i++)
		{
			_relay->poll_messages();

			std::this_thread::sleep_for(10ms);
		}

		unregister_callbacks();
	}

	void server_to_client_connection_factory::set_remote_sdp(hololight::signaling::webrtc::Sdp & sdp)
	{
		SetRemoteDescription(_peerConnectionId, "answer", sdp.content.c_str());
	}

	void server_to_client_connection_factory::set_remote_ice_cand(hololight::signaling::webrtc::IceCandidate & cand)
	{
		AddIceCandidate(_peerConnectionId, cand.candidate.c_str(), cand.sdp_mlineindex, cand.sdp_mid.c_str());
	}

	void server_to_client_connection_factory::OnLocalSDPReady(const char * type, const char * sdp, void * userData)
	{
		auto instance = static_cast<server_to_client_connection_factory*>(userData);

		hololight::signaling::webrtc::Sdp offer {
			hololight::signaling::webrtc::SdpType::offer,
			std::string { sdp } };

		instance->_offerPromise.set_value(offer);
	}

	void server_to_client_connection_factory::OnIceCandidateReadyToSend(const char * candidate, const int sdp_mline_index, const char * sdp_mid, void * userData)
	{
		auto instance = static_cast<server_to_client_connection_factory*>(userData);

		IceCandidate cand{ std::string(candidate), std::string(sdp_mid), sdp_mline_index };

		instance->send_ice_candidate(cand);
	}
}