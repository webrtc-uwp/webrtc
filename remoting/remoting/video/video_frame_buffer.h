#ifndef HOLOLIGHT_REMOTING_VIDEO_VIDEO_FRAME_BUFFER_H
#define HOLOLIGHT_REMOTING_VIDEO_VIDEO_FRAME_BUFFER_H

#include "video_frame.h"

#include <queue>
#include <mutex>
#include <functional>
#include <atomic>

namespace hololight::remoting::video {

	// TODO: Replace this class with something more sensible/flexible. WebRTC probably has someting like that, but I'm too lazy to look.
	class video_frame_buffer
	{
	public:
		void push(const uint8_t* data_y, const uint8_t* data_u, const uint8_t* data_v, const uint8_t* data_a, hololight::remoting::video::video_frame_desc& frameDesc);
		void wait_for_frame_and_exec(std::function<void(video_frame*)> f);

		bool try_exec_with_frame(std::function<void(hololight::remoting::video::video_frame*)> f);

		video_frame_desc peek_frame_desc();

	private:
		
		std::mutex _frameLock;
		video_frame _frame;
		std::atomic_bool _newFrameAvailable;
	};

}

#endif // HOLOLIGHT_REMOTING_VIDEO_VIDEO_FRAME_BUFFER_H