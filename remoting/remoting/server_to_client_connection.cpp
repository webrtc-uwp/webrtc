
#include "server_to_client_connection.h"
#include "../unity_plugin_apis.h"
#include "rtc_base/logging.h"
#include <chrono>
#include <cassert>

using namespace std::chrono_literals;

hololight::remoting::server_to_client_connection::server_to_client_connection(int peerConnectionId) :
	_peerConnectionId(peerConnectionId)
{
	RegisterOnDataFromDataChannelReady(_peerConnectionId, &server_to_client_connection::OnDataFromDatachannel, this);
}

hololight::remoting::server_to_client_connection::~server_to_client_connection() {

	ClosePeerConnection(_peerConnectionId);
}

void hololight::remoting::server_to_client_connection::send_frame(ID3D11Texture2D* frame)
{
	assert(0 <= _peerConnectionId);

	if (OnD3DFrame(_peerConnectionId, frame)) {
		RTC_LOG(LS_INFO) << "Sending frame succeeded!";
	}
	else
	{
		RTC_LOG(LS_INFO) << "Sending frame failed!";
	}
}

std::optional<std::string> hololight::remoting::server_to_client_connection::poll_next_input(int maxWaitTimeMillis)
{
	RTC_LOG(LS_INFO) << "Waiting for pose...";

	auto startTime = std::chrono::high_resolution_clock::now();

	do {

		{
			std::lock_guard<std::mutex> lock(_poseLock);

			if (_pose.has_value()) {
				
				RTC_LOG(LS_INFO) << " and got it!";

				auto pose = _pose.value();

				_pose = std::nullopt;

				return std::optional<std::string>{ pose };
			}
		}

		std::this_thread::yield();
	} while ( (std::chrono::high_resolution_clock::now() - startTime) / std::chrono::milliseconds(1) < maxWaitTimeMillis);

	return std::nullopt;
}

void hololight::remoting::server_to_client_connection::OnDataFromDatachannel(const char * msg, void * userData)
{
	auto instance = static_cast<server_to_client_connection*>(userData);

	{
		std::lock_guard<std::mutex> lock(instance->_poseLock);

		instance->_pose = std::string(msg);
	}
}
