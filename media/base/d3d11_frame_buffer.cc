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
        staging_texture_.copy_from(staging_texture);
        rendered_image_.copy_from(rendered_image);
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
        //TODO: what if we need subresource indices or something? maybe we should allow passing in
        //a config struct into this method. Or rather class, since this is an override, so we can't pass params.
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
            int stride_y = width_;
            int stride_uv = stride_y / 2;

            //TODO: this very much depends on the texture format. We should make clear which ones we support and either
            //check at creation time or somewhere else.
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
                RTC_LOG(LS_ERROR) << "i420 conversion failed with error code" << result;
            }

            return I420Buffer::Copy(width_, height_, dst_y_, stride_y, dst_u_, stride_uv, dst_v_, stride_uv);
        }

        return nullptr;
    }
}
