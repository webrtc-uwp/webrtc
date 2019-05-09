#include "video_frame_buffer.h"
#include "rtc_base/logging.h"

namespace hololight::remoting::video {

	void video_frame_buffer::push(const uint8_t* data_y, const uint8_t* data_u, const uint8_t* data_v, const uint8_t* data_a, hololight::remoting::video::video_frame_desc& frameDesc)
	{
		if (_frameLock.try_lock()) {

			_frame.replace_with(data_y, data_u, data_v, data_a, frameDesc);
			_newFrameAvailable = true;

			_frameLock.unlock();
		}
		else {
			RTC_LOG(LS_INFO) << "Dropping frame because frame buffer was still being read while it arrived!\n";
		}
	}

	void video_frame_buffer::wait_for_frame_and_exec(std::function<void(video_frame*)> f)
	{
		while (!_newFrameAvailable) 
		{
			std::this_thread::yield(); // TODO: Find a better alternative to doing this

		}

		{
			std::lock_guard<std::mutex> lock(_frameLock);

			f(&_frame);

			_newFrameAvailable = false;
		}
	}

	bool video_frame_buffer::try_exec_with_frame(std::function<void(hololight::remoting::video::video_frame*)> f)
	{
		if (_newFrameAvailable) {
			std::lock_guard<std::mutex> lock(_frameLock);

			f(&_frame);

			_newFrameAvailable = false;

			return true;
		}

		return false;
	}

	// TODO: This function is useful for initializing resolution based resources before the actual frame handling code. Maybe there is a better solution for this?
	video_frame_desc video_frame_buffer::peek_frame_desc()
	{
		std::lock_guard<std::mutex> lock(_frameLock);
		
		if (!_newFrameAvailable) {
			throw new std::exception("Tried peeking at empty frame queue"); // TODO: Get rid of this and think about a better solution for error handling
		}
		else {
			return _frame._desc;
		}
	}
}
