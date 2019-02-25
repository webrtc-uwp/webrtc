#include "d3d11_frame_buffer.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include "rtc_base/logging.h"
#include "api/video/i420_buffer.h"

using namespace webrtc;

namespace hololight {
    rtc::scoped_refptr<D3D11VideoFrameBuffer> D3D11VideoFrameBuffer::Create(ID3D11DeviceContext* context, ID3D11Texture2D* staging_texture, ID3D11Texture2D* rendered_image, int width, int height) {
        return new rtc::RefCountedObject<D3D11VideoFrameBuffer>(context, staging_texture, rendered_image, width, height);
    }

    rtc::scoped_refptr<D3D11VideoFrameBuffer> D3D11VideoFrameBuffer::Create(ID3D11DeviceContext* context, 
        ID3D11Texture2D* staging_texture, ID3D11Texture2D* rendered_image, int width, int height, uint8_t* dst_y, uint8_t* dst_u, uint8_t* dst_v) {
            return new rtc::RefCountedObject<D3D11VideoFrameBuffer>(context, staging_texture, rendered_image, width, height, dst_y, dst_u, dst_v);
    }

    VideoFrameBuffer::Type D3D11VideoFrameBuffer::type() const {
        return VideoFrameBuffer::Type::kNative;
    }

    D3D11VideoFrameBuffer::D3D11VideoFrameBuffer(ID3D11DeviceContext* context, ID3D11Texture2D* staging_texture, ID3D11Texture2D* rendered_image, int width, int height) 
    : width_(width), height_(height) {
        //set some useful stuff here, i.e. create the staging texture
        winrt::com_ptr<ID3D11Texture2D> tmp;
        tmp.copy_from(staging_texture);
        staging_texture_ = tmp;

        tmp.copy_from(rendered_image);
        rendered_image_ = tmp;

        context_.copy_from(context);
    }

    D3D11VideoFrameBuffer::D3D11VideoFrameBuffer(ID3D11DeviceContext* context, 
        ID3D11Texture2D* staging_texture, ID3D11Texture2D* rendered_image, int width, 
        int height, uint8_t* dst_y, uint8_t* dst_u, uint8_t* dst_v) :
        width_(width), height_(height), dst_y_(dst_y), dst_u_(dst_u), dst_v_(dst_v) {
            context_.copy_from(context);
            staging_texture_.copy_from(staging_texture);
            rendered_image_.copy_from(rendered_image);
        }

    rtc::scoped_refptr<webrtc::I420BufferInterface> D3D11VideoFrameBuffer::ToI420() {
        //if we move copying to here we actually need the context in this class...
        //fuck man, this is annoying.

        //what if we need subresource indices or something? maybe we should allow passing in
        //a config struct into this method
        context_->CopySubresourceRegion(
            staging_texture_.get(),
            0,
            0,
            0,
            0,
            rendered_image_.get(),
            0,
            nullptr);

        D3D11_MAPPED_SUBRESOURCE mapped;
        HRESULT hr = context_->Map(staging_texture_.get(), 0, D3D11_MAP_READ, 0, &mapped);

        if (SUCCEEDED(hr)) {
            //if this fails (and I very much doubt there is no off-by-one error here)
            //lookie here: https://groups.google.com/forum/#!topic/discuss-libyuv/iLr_IwLo-rk

            int stride_y = width_;
            int stride_uv = stride_y / 2; //I420 u and v planes are usually half of y, or so I've heard

            int result = libyuv::ARGBToI420(
                static_cast<uint8_t*>(mapped.pData), 
                mapped.RowPitch, 
                dst_y_,
                width_,
                dst_u_,
                stride_uv,
                dst_v_,
                stride_uv,
                width_,
                height_);
            assert(result == 0);
            context_->Unmap(staging_texture_.get(), 0);

            if (result != 0) {
                RTC_LOG(LS_ERROR) << "yo, something's fucked up with i420 conversion";
            }

            //create I420Buffer and return or something similar.
            //there's also copy.
            return I420Buffer::Copy(width_, height_, dst_y_, stride_y, dst_u_, stride_uv, dst_v_, stride_uv);
        }

        return nullptr;
    }
}
