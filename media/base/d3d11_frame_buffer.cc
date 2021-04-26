/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "d3d11_frame_buffer.h"
#include "api/video/i420_buffer.h"
#include "common_video/include/video_frame_buffer.h"
#include "rtc_base/logging.h"
#include "third_party/libyuv/include/libyuv/convert.h"

using namespace webrtc;

namespace hlr {
rtc::scoped_refptr<D3D11VideoFrameBuffer> D3D11VideoFrameBuffer::Create(
    ID3D11DeviceContext* context,
    ID3D11Texture2D* staging_texture,
    ID3D11Texture2D* rendered_image,
    int width,
    int height,
    DXGI_FORMAT format) {
  return new rtc::RefCountedObject<D3D11VideoFrameBuffer>(
      context, staging_texture, rendered_image, width, height, format);
}

rtc::scoped_refptr<D3D11VideoFrameBuffer> D3D11VideoFrameBuffer::Create(
    ID3D11DeviceContext* context,
    ID3D11Texture2D* staging_texture,
    ID3D11Texture2D* rendered_image,
    uint8_t* dst_y,
    uint8_t* dst_u,
    uint8_t* dst_v,
    D3D11_TEXTURE2D_DESC rendered_image_desc) {
  return new rtc::RefCountedObject<D3D11VideoFrameBuffer>(
      context, staging_texture, rendered_image, dst_y, dst_u, dst_v,
      rendered_image_desc);
}

VideoFrameBuffer::Type D3D11VideoFrameBuffer::type() const {
  return VideoFrameBuffer::Type::kNative;
}

D3D11VideoFrameBuffer::D3D11VideoFrameBuffer(ID3D11DeviceContext* context,
                                             ID3D11Texture2D* staging_texture,
                                             ID3D11Texture2D* rendered_image,
                                             int width,
                                             int height,
                                             DXGI_FORMAT format)
    : width_(width),
      height_(height),
      dst_y_(nullptr),
      dst_u_(nullptr),
      dst_v_(nullptr),
      texture_format_(format) {
  RTC_CHECK(rendered_image != nullptr);
  staging_texture_.copy_from(staging_texture);
  rendered_image_.copy_from(rendered_image);
  context_.copy_from(context);
}

D3D11VideoFrameBuffer::D3D11VideoFrameBuffer(
    ID3D11DeviceContext* context,
    ID3D11Texture2D* staging_texture,
    ID3D11Texture2D* rendered_image,
    uint8_t* dst_y,
    uint8_t* dst_u,
    uint8_t* dst_v,
    D3D11_TEXTURE2D_DESC rendered_image_desc)
    : dst_y_(dst_y),
      dst_u_(dst_u),
      dst_v_(dst_v),
      rendered_image_desc_(rendered_image_desc) {
  // instead of getting width, height and format as args we could pass desc
  // (staging or rendered?) and set members based on that here
  RTC_CHECK(rendered_image != nullptr);
  context_.copy_from(context);
  staging_texture_.copy_from(staging_texture);
  rendered_image_.copy_from(rendered_image);

  if (rendered_image_desc.ArraySize == 2) {
    width_ = rendered_image_desc.Width * 2;
  } else {
    width_ = rendered_image_desc.Width;
  }

  height_ = rendered_image_desc.Height;
  texture_format_ = rendered_image_desc.Format;
}

D3D11VideoFrameBuffer::~D3D11VideoFrameBuffer() {}

int D3D11VideoFrameBuffer::width() const {
  return width_;
}

int D3D11VideoFrameBuffer::height() const {
  return height_;
}

// Copies contents of rendered_image_ to staging_texture_, then downloads it to
// the CPU and converts to i420 via libyuv.
rtc::scoped_refptr<webrtc::I420BufferInterface>
D3D11VideoFrameBuffer::ToI420() {
  // if the user didn't pass in temp buffers, we don't support converting to
  // I420.
  RTC_CHECK(dst_y_ != nullptr);
  RTC_CHECK(dst_u_ != nullptr);
  RTC_CHECK(dst_v_ != nullptr);

  if (rendered_image_desc_.ArraySize == 1) {
    // Double-wide texture
    context_->CopySubresourceRegion(staging_texture_.get(), 0, 0, 0, 0,
                                    rendered_image_.get(), subresource_index_,
                                    nullptr);

  } else if (rendered_image_desc_.ArraySize == 2) {
    // Texture array (2 images)
    UINT left_eye_subresource =
        D3D11CalcSubresource(0, 0, rendered_image_desc_.MipLevels);
    UINT right_eye_subresource =
        D3D11CalcSubresource(0, 1, rendered_image_desc_.MipLevels);

    context_->CopySubresourceRegion(staging_texture_.get(), 0, 0, 0, 0,
                                    rendered_image_.get(), left_eye_subresource,
                                    nullptr);

    context_->CopySubresourceRegion(
        staging_texture_.get(), 0, rendered_image_desc_.Width, 0, 0,
        rendered_image_.get(), right_eye_subresource, nullptr);
  } else {
    RTC_LOG(LS_WARNING)
        << "Got frame with ArraySize > 2, not sure what to do with this";
    // send black frame maybe? I think there were methods for that.
    // or we use the subresource index that has been set in the decoder (in case
    // of client), copy that to staging and whatever. Then again, if the decoder
    // allocates just 2 textures we treat them as eyes even though they're not.
    // Not sure what the best solution is here.
  }

  D3D11_MAPPED_SUBRESOURCE mapped;
  HRESULT hr =
      context_->Map(staging_texture_.get(), 0, D3D11_MAP_READ, 0, &mapped);

  if (SUCCEEDED(hr)) {
    int stride_y = width_;
    int stride_uv = stride_y / 2;

    if (texture_format_ == DXGI_FORMAT_R8G8B8A8_UNORM ||
        texture_format_ == DXGI_FORMAT_R8G8B8A8_TYPELESS) {
      int32_t conversion_result = libyuv::ARGBToI420(
          static_cast<uint8_t*>(mapped.pData), mapped.RowPitch, dst_y_, width_,
          dst_u_, stride_uv, dst_v_, stride_uv, width_, height_);

      if (conversion_result != 0) {
        RTC_LOG(LS_ERROR) << "i420 conversion failed with error code"
                          << conversion_result;
        // crash in debug mode so we can find out what went wrong
        RTC_DCHECK(conversion_result == 0);
      }
    } else if (texture_format_ == DXGI_FORMAT_R16_TYPELESS) {
      // This is uint16_t because we have a 16-bpp texture format
      uint16_t* pixel = reinterpret_cast<uint16_t*>(mapped.pData);
      // RowPitch is in bytes...so if we want it in units of 16-bits, divide by 2
      uint32_t row_pitch_16 = mapped.RowPitch / 2;

      for (int col = 0; col < height_; col++) {
        for (uint32_t i = 0; i < row_pitch_16; i++) {
          uint8_t high = *pixel >> 8;
          uint8_t low = static_cast<uint8_t>(*pixel);

          // write high 8 bits into Y plane
          dst_y_[i] = high;

          // smoke test: having empty uv crashes inside the encoder, because of
          // course it does. split low into 4-bit values, zero-extend them and
          // write into u and v
          uint8_t high_nibble_mask = 0xF0;
          uint8_t low_nibble_mask = 0x0F;

          uint8_t hn = low & high_nibble_mask;
          uint8_t ln = low & low_nibble_mask;

          dst_u_[i] = hn;
          dst_v_[i] = ln;

          pixel++;
        }
      }

      // TODO: print all the bytes after packing the planes into YUV

    } else {
      FATAL() << "Unsupported texture format";
    }

    context_->Unmap(staging_texture_.get(), 0);

    rtc::Callback0<void> unused;
    return webrtc::WrapI420Buffer(width_, height_, dst_y_, stride_y, dst_u_,
                                  stride_uv, dst_v_, stride_uv, unused);
  }

  return nullptr;
}
}  // namespace hlr
