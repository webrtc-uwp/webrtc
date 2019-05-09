#ifndef HOLOLIGHT_REMOTING_SERVER_TO_CLIENT_CONNECTION_H
#define HOLOLIGHT_REMOTING_SERVER_TO_CLIENT_CONNECTION_H

#include "../signaling/webrtc/webrtc_signaling_relay.h"
#include <d3d11.h>
#include <string>
#include <optional>

// This queue and locking primitives should maybe move somewhere else
#include <mutex>
#include <queue>

namespace hololight::remoting
{
	class server_to_client_connection
	{
	public:
		server_to_client_connection(int peerConnectionId);
		~server_to_client_connection();

		void send_frame(ID3D11Texture2D* frame);
		std::optional<std::string> poll_next_input(int maxWaitTimeMillis); // TODO: Change signature to something better

	private:
		int _peerConnectionId = -1;

		static void OnDataFromDatachannel(const char* msg, void* userData);

		// This queue and locking primitives should maybe move somewhere else
		std::mutex _poseLock;
		std::optional<std::string> _pose;
	};
}

#endif