
#include "client_to_server_connection.h"

#include "../unity_plugin_apis.h"

hololight::remoting::client_to_server_connection::client_to_server_connection(
	int peerConnectionId,
	std::unique_ptr<hololight::remoting::video::video_frame_buffer> frameBuffer) :
	_peerConnectionId(peerConnectionId), _frameBuffer(std::move(frameBuffer))
{
	RegisterOnRemoteI420FrameReady(peerConnectionId, &client_to_server_connection::OnRemoteFrameReady, this);
}

hololight::remoting::client_to_server_connection::~client_to_server_connection() {

	ClosePeerConnection(_peerConnectionId);
}

void hololight::remoting::client_to_server_connection::send_input(std::string message)
{
	SendDataViaDataChannel(_peerConnectionId, message.c_str());
}

void hololight::remoting::client_to_server_connection::wait_for_frame_and_exec(std::function<void(hololight::remoting::video::video_frame*)> f)
{
	// OutputDebugStringW(L"Waiting for frame...\n");

	_frameBuffer->wait_for_frame_and_exec(f);
}

bool hololight::remoting::client_to_server_connection::try_exec_with_frame(std::function<void(hololight::remoting::video::video_frame*)> f)
{
	//OutputDebugStringW(L"Trying to get newest frame...");

	auto got_frame = _frameBuffer->try_exec_with_frame(f);

	//if (got_frame) {
	//	OutputDebugStringW(L"Success\n");
	//}
	//else {
	//	OutputDebugStringW(L"but couldn't get one!\n");
	//}

	return got_frame;
}

hololight::remoting::video::video_frame_desc hololight::remoting::client_to_server_connection::peek_frame_desc()
{
	return _frameBuffer->peek_frame_desc();
}

void hololight::remoting::client_to_server_connection::OnRemoteFrameReady(const uint8_t * data_y, const uint8_t * data_u, const uint8_t * data_v, const uint8_t * data_a, int stride_y, int stride_u, int stride_v, int stride_a, uint32_t width, uint32_t height, void * userData)
{
	auto instance = static_cast<client_to_server_connection*>(userData);

	hololight::remoting::video::video_frame_desc frame_desc{
		width,
		height,
		stride_y,
		stride_u,
		stride_v,
		stride_a
	};

	instance->_frameBuffer->push(data_y, data_u, data_v, data_a, frame_desc);
}
