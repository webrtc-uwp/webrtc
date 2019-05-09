#ifndef HOLOLIGHT_REMOTING_CLIENT_TO_SERVER_CONNECTION_H
#define HOLOLIGHT_REMOTING_CLIENT_TO_SERVER_CONNECTION_H

#include "../signaling/webrtc/webrtc_signaling_relay.h"
#include "video/video_frame_buffer.h"
#include <string>

namespace hololight::remoting
{
	class client_to_server_connection
	{
	public:
		client_to_server_connection(
			int peerConnectionId,
			std::unique_ptr<hololight::remoting::video::video_frame_buffer> frameBuffer);
		~client_to_server_connection();

		void send_input(std::string message); // TODO: Allow binary messages
		void wait_for_frame_and_exec(std::function<void(hololight::remoting::video::video_frame*)> f);
		
		// False when no frame available
		bool try_exec_with_frame(std::function<void(hololight::remoting::video::video_frame*)> f);

		// TODO: Find a better solution for this
		hololight::remoting::video::video_frame_desc peek_frame_desc();

	private:
		int _peerConnectionId;
		
		std::unique_ptr<hololight::remoting::video::video_frame_buffer> _frameBuffer;

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
	};
}

#endif