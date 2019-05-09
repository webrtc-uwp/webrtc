
#include "client_to_server_connection_factory.h"

#include <cassert>
#include "rtc_base/logging.h"
#include "../../unity_plugin_apis.h"
#include "../../util/future_utils.h"
#include <chrono>
#include <thread>

using namespace std::chrono_literals;

namespace hololight::remoting::factories {
	client_to_server_connection_factory::client_to_server_connection_factory(std::unique_ptr<hololight::signaling::webrtc::WebrtcSignalingRelay> relay) :
		connection_factory_base(std::move(relay))
	{
	}

	std::unique_ptr<hololight::remoting::client_to_server_connection> client_to_server_connection_factory::create_connection()
	{
		create_peerconnection();

		wait_until_remote_offer_set();

		auto answer = create_and_set_answer();
		send_sdp(answer);

		wait_until_connection_ready();

		RTC_LOG(LS_INFO) << "C2S Connection established! HOLO YAY";

		return std::make_unique<hololight::remoting::client_to_server_connection>(
			_peerConnectionId,
			std::move(_videoFrameBuffer));
	}

	///////////////// Stupid implementation using the C API. This should be replaced by Kris' nice Connection class

	void client_to_server_connection_factory::create_peerconnection()
	{
		_peerConnectionId = CreatePeerConnection(
			nullptr,
			0,
			nullptr,
			nullptr,
			true);

		assert(0 <=_peerConnectionId); // We do this here once, but should probably do it in every function

		register_callbacks();
	}

	void client_to_server_connection_factory::register_callbacks()
	{
		RegisterOnRemoteI420FrameReady(_peerConnectionId, &client_to_server_connection_factory::OnRemoteFrameReady, this);
		RegisterOnLocalSdpReadytoSend(_peerConnectionId, &client_to_server_connection_factory::OnLocalSDPReady, this);
		RegisterOnIceCandiateReadytoSend(_peerConnectionId, &client_to_server_connection_factory::OnLocalICECandidate, this);
	}

	void client_to_server_connection_factory::unregister_callbacks()
	{
		RegisterOnRemoteI420FrameReady(_peerConnectionId, nullptr, nullptr);
		RegisterOnLocalSdpReadytoSend(_peerConnectionId, nullptr, nullptr);
		RegisterOnIceCandiateReadytoSend(_peerConnectionId, nullptr, nullptr);
	}

	void client_to_server_connection_factory::wait_until_remote_offer_set()
	{
		auto fut = _offerSetPromise.get_future();

		while (!hololight::util::future_utils::is_ready(fut)) {

			_relay->poll_messages();

			std::this_thread::yield();
		}

		fut.wait();
	}

	hololight::signaling::webrtc::Sdp client_to_server_connection_factory::create_and_set_answer()
	{
		CreateAnswer(_peerConnectionId);

		auto fut = _answerPromise.get_future();

		while (!hololight::util::future_utils::is_ready(fut))
		{
			_relay->poll_messages();

			std::this_thread::yield();
		}

		auto answer = fut.get();

		return answer;
	}

	void client_to_server_connection_factory::wait_until_connection_ready()
	{
		auto fut = _remoteFrameReady.get_future();

		while (!hololight::util::future_utils::is_ready(fut))
		{
			_relay->poll_messages();

			std::this_thread::yield();
		}

		fut.get();

		// for (int i = 0; i < 300; i++)
		// {
		// 	_relay->poll_messages();

		// 	std::this_thread::sleep_for(10ms);
		// }

		unregister_callbacks();
	}

	void client_to_server_connection_factory::set_remote_sdp(hololight::signaling::webrtc::Sdp & sdp)
	{
		// We set the offer here
		SetRemoteDescription(_peerConnectionId, "offer", sdp.content.c_str());

		try
		{
			_offerSetPromise.set_value();
		}
		catch (const std::future_error&)
		{
			RTC_LOG(LS_INFO) << "Fatal error while trying to mark arrival of remote SDP asynchronously";
		}
		
	}

	void client_to_server_connection_factory::set_remote_ice_cand(hololight::signaling::webrtc::IceCandidate & cand)
	{
		AddIceCandidate(_peerConnectionId, cand.candidate.c_str(), cand.sdp_mlineindex, cand.sdp_mid.c_str());
	}

	void client_to_server_connection_factory::OnLocalSDPReady(const char * type, const char * sdp, void * userData)
	{
		auto instance = static_cast<client_to_server_connection_factory*>(userData);

		hololight::signaling::webrtc::Sdp sigSdp { 
			hololight::signaling::webrtc::SdpType::answer,
			std::string {sdp} };

		instance->_answerPromise.set_value(sigSdp);
	}

	void client_to_server_connection_factory::OnRemoteFrameReady(
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
		void* userData)
	{
		auto instance = static_cast<client_to_server_connection_factory*>(userData);

		if (instance->_receivedFirstFrame) {
			return;
		}

		instance->_receivedFirstFrame = true;

		instance->_videoFrameBuffer = std::make_unique<hololight::remoting::video::video_frame_buffer>();

		hololight::remoting::video::video_frame_desc frame_desc{
			width,
			height,
			stride_y,
			stride_u,
			stride_v,
			stride_a
		};

		instance->_videoFrameBuffer->push(data_y, data_u, data_v, data_a, frame_desc);

		instance->_remoteFrameReady.set_value();
	}

	void client_to_server_connection_factory::OnLocalICECandidate(const char * candidate, const int sdp_mline_index, const char * sdp_mid, void * userData)
	{
		auto instance = static_cast<client_to_server_connection_factory*>(userData);

		hololight::signaling::webrtc::IceCandidate cand {
			std::string(candidate),
			std::string(sdp_mid),
			sdp_mline_index
		};

		instance->send_ice_candidate(cand);
	}
}