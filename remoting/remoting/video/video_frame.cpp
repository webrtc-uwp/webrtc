#include "video_frame.h"

#include <cstring>

namespace hololight::remoting::video {

	video_frame::video_frame()
	{
		_data_y = nullptr;
		_data_u = nullptr;
		_data_v = nullptr;
		_data_a = nullptr;
	}

	video_frame::~video_frame()
	{
		if (_data_y)
			delete _data_y;

		if (_data_u)
			delete _data_u;

		if (_data_v)
			delete _data_v;

		if (_data_a)
			delete _data_a;
	}

	// only resizes when necessary
	void video_frame::replace_with(
		const uint8_t * data_y, 
		const uint8_t * data_u, 
		const uint8_t * data_v, 
		const uint8_t * data_a, 
		video_frame_desc& desc)
	{	
		ensure_frame_size(desc);

		uint32_t halfheight = desc.height >> 1;
		memcpy(this->_data_y, data_y, desc.stride_y * desc.height);
		memcpy(this->_data_u, data_u, desc.stride_u * halfheight);
		memcpy(this->_data_v, data_v, desc.stride_v * halfheight);

		if (desc.stride_a > 0) {
			memcpy(this->_data_a, data_a, desc.stride_a * desc.height); // TODO: Figure out if alpha is sent at full res or not
		}

		_desc = desc;
	}

	void video_frame::ensure_frame_size(video_frame_desc & desc)
	{
		int current_size_y = _desc.stride_y * _desc.height;
		int requested_size_y = desc.stride_y * desc.height;

		ensure_buffer_size(&_data_y, current_size_y, requested_size_y);

		int current_size_u = _desc.stride_u * _desc.height;
		int requested_size_u = desc.stride_u * desc.height;

		ensure_buffer_size(&_data_u, current_size_u, requested_size_u);

		int current_size_v = _desc.stride_v * _desc.height;
		int requested_size_v = desc.stride_v * desc.height;

		ensure_buffer_size(&_data_v, current_size_v, requested_size_v);

		int current_size_a = _desc.stride_a * _desc.height;
		int requested_size_a = desc.stride_a * desc.height;

		ensure_buffer_size(&_data_a, current_size_a, requested_size_a);
	}

	void video_frame::ensure_buffer_size(uint8_t** buffer, int current_size, int requested_size)
	{
		if (0 < requested_size && (current_size < requested_size || !*buffer) ) {
			delete *buffer;
			*buffer = new uint8_t[requested_size];
		}
	}
}