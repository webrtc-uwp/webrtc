#ifndef HOLOLIGHT_REMOTING_VIDEO_VIDEO_FRAME_H
#define HOLOLIGHT_REMOTING_VIDEO_VIDEO_FRAME_H

#include <cstdint>

namespace hololight::remoting::video {

	struct video_frame_desc {

		uint32_t width;
		uint32_t height;

		int stride_y;
		int stride_u;
		int stride_v;
		int stride_a;

	};

	struct video_frame
	{
		video_frame();
		~video_frame();

		void replace_with(
			const uint8_t* data_y,
			const uint8_t* data_u,
			const uint8_t* data_v,
			const uint8_t* data_a,
			video_frame_desc& desc);

		video_frame_desc _desc;

		uint8_t* _data_y = nullptr;
		uint8_t* _data_u = nullptr;
		uint8_t* _data_v = nullptr;
		uint8_t* _data_a = nullptr;

	private:
		void ensure_frame_size(video_frame_desc& desc);

		void ensure_buffer_size(uint8_t** pBuffer, int current_size, int requested_size);
	};
}

#endif // HOLOLIGHT_REMOTING_VIDEO_VIDEO_FRAME_H