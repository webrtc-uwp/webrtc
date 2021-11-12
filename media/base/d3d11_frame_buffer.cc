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
#include "third_party/libyuv/include/libyuv/convert_from_argb.h"

#define ON_SUCCEEDED(act) if (SUCCEEDED(hr)) { hr = act; if (FAILED(hr)) { RTC_LOG(LS_WARNING) << "ERROR:" << #act; } }

using namespace webrtc;

namespace hlr {

rtc::scoped_refptr<D3D11VideoFrameBuffer> D3D11VideoFrameBuffer::Create(
    ID3D11DeviceContext* context,
    ID3D11Texture2D* color_texture, ID3D11Texture2D* color_staging_texture,
    UINT subresource_index, UINT subresource_width, UINT subresource_height) {
  return new rtc::RefCountedObject<D3D11VideoFrameBuffer>(
      context, color_texture, color_staging_texture, subresource_index, subresource_width, subresource_height);
}

rtc::scoped_refptr<D3D11VideoFrameBuffer> D3D11VideoFrameBuffer::Create(
    ID3D11DeviceContext* context,
    ID3D11Texture2D* color_texture, ID3D11Texture2D* color_staging_texture,
    ID3D11Texture2D* depth_texture, ID3D11Texture2D* depth_staging_texture, ID3D11Texture2D* depth_staging_texture_array) {
  return new rtc::RefCountedObject<D3D11VideoFrameBuffer>(
      context,
      color_texture, color_staging_texture,
      depth_texture, depth_staging_texture, depth_staging_texture_array);
}

VideoFrameBuffer::Type D3D11VideoFrameBuffer::type() const {
  return VideoFrameBuffer::Type::kNative;
}

D3D11VideoFrameBuffer::D3D11VideoFrameBuffer(
    ID3D11DeviceContext* context,
    ID3D11Texture2D* color_texture, ID3D11Texture2D* color_staging_texture,
    UINT subresource_index, UINT subresource_width, UINT subresource_height)
    : subresource_index_(subresource_index),
      subresource_width_(subresource_width),
      subresource_height_(subresource_height),
      width_(subresource_width),
      height_(subresource_height)
{
  RTC_CHECK(color_texture != nullptr);
  context_.copy_from(context);
  color_texture_.copy_from(color_texture);
  color_staging_texture_.copy_from(color_staging_texture);

  D3D11_TEXTURE2D_DESC color_texture_desc = {};
  color_texture->GetDesc(&color_texture_desc);
  color_texture_desc_ = color_texture_desc;

  // color_texture_format_ = color_texture_desc.Format;
  if (color_texture_desc.ArraySize == 2) {
    width_ = subresource_width_ *= 2;
  }
  // height_ = color_texture_desc.Height;

  depth_texture_ = nullptr;
  depth_staging_texture_ = nullptr;
  depth_texture_desc_ = {};
}

D3D11VideoFrameBuffer::D3D11VideoFrameBuffer(
    ID3D11DeviceContext* context,
    ID3D11Texture2D* color_texture, ID3D11Texture2D* color_staging_texture,
    ID3D11Texture2D* depth_texture, ID3D11Texture2D* depth_staging_texture, ID3D11Texture2D* depth_staging_texture_array)
    : subresource_index_(0) {
  RTC_CHECK(color_texture != nullptr);
  context_.copy_from(context);
  color_staging_texture_.copy_from(color_staging_texture);
  color_texture_.copy_from(color_texture);
  depth_staging_texture_.copy_from(depth_staging_texture);
  depth_staging_texture_array_.copy_from(depth_staging_texture_array);
  depth_texture_.copy_from(depth_texture);

  D3D11_TEXTURE2D_DESC color_texture_desc = {};
  color_texture->GetDesc(&color_texture_desc);
  color_texture_desc_ = color_texture_desc;

  width_ = subresource_width_ = color_texture_desc.Width;
  if (color_texture_desc.ArraySize == 2) {
    width_ = subresource_width_ *= 2;
  }

  height_ = subresource_height_ = color_texture_desc.Height;

  depth_texture_desc_ = {};
  if (depth_texture != nullptr) {
    D3D11_TEXTURE2D_DESC depth_desc = {};
    depth_texture->GetDesc(&depth_desc);
    depth_texture_desc_ = depth_desc;
    height_ *= 2;
  }
}

D3D11VideoFrameBuffer::~D3D11VideoFrameBuffer() {}

int D3D11VideoFrameBuffer::width() const {
  return width_;
}

int D3D11VideoFrameBuffer::height() const {
  return height_;
}

// Copies contents of color_texture_ to color_staging_texture_, then downloads it
// to the CPU and converts to i420 via libyuv.
rtc::scoped_refptr<webrtc::I420BufferInterface>
D3D11VideoFrameBuffer::ToI420() {
  FATAL() << "this interface is shit and forces us to create a new buffer for every call instead of passing it as a parameter";
  return nullptr;
}

// TODO: (move staging texture out of here and) create a Use method with a predicate to map/unmap internally
// Stages, downloads and converts texture to i420
rtc::scoped_refptr<webrtc::I420BufferInterface>
D3D11VideoFrameBuffer::ToI420(uint8_t* buffer, int capacity) {
  RTC_CHECK(buffer != nullptr);

  const int frame_width = subresource_width_;
  const int frame_height = subresource_height_;

  const int& width_y = frame_width;
  const int& height_y = frame_height;
  const int& stride_y = width_y;
  const int size_y = width_y * height_y;

  const int width_u = div_ceiled_fast(width_y, 2);
  const int height_u = div_ceiled_fast(height_y, 2);
  const int stride_u = width_u;
  const int size_u = width_u * height_u;

  // const int& width_v = width_u;
  // const int& height_v = height_u;
  const int& stride_v = stride_u;
  const int& size_v = size_u;
  const int size_yuv = size_y + size_u + size_v;

  const int has_depth_alpha = (int)(depth_texture_.get() != nullptr);
  RTC_CHECK(capacity >= size_yuv * (1 + has_depth_alpha));

  uint8_t* dst_y  = buffer;
  uint8_t* dst_dh = dst_y + (size_y * has_depth_alpha);

  uint8_t* dst_u  = dst_dh + size_y;
  uint8_t* dst_dl = dst_u + (size_u * has_depth_alpha);
  uint8_t* dst_v  = dst_dl + size_u;
  // uint8_t* dst_a  = dst_v + (size_v * has_depth_alpha);

  auto convertToI420 = [&](void* src_data, int src_row_pitch, int /* src_depth_pitch */) {
    if (!(color_texture_desc_.Format == DXGI_FORMAT_R8G8B8A8_UNORM
       || color_texture_desc_.Format == DXGI_FORMAT_R8G8B8A8_TYPELESS)) {
      FATAL() << "Unsupported color texture format";
      return;
    }

    uint8_t* src_data8 = static_cast<uint8_t*>(src_data);
    int conversion_result = libyuv::ARGBToI420(
        src_data8, src_row_pitch,
        OUT dst_y, stride_y,
        OUT dst_u, stride_u,
        OUT dst_v, stride_v,
        frame_width, frame_height);

    if (conversion_result != 0) {
      RTC_LOG(LS_ERROR) << "i420 conversion failed with error code" << conversion_result;
      // crash in debug mode so we can find out what went wrong
      RTC_DCHECK(conversion_result == 0);
    }
  };

  StageTextures();

  HRESULT hr = S_OK;
  ON_SUCCEEDED(UseStagedResource(color_staging_texture_.get(), convertToI420));
  if(SUCCEEDED(hr))
  {
    rtc::Callback0<void> unused;
    // TODO: not needed, see ToNV12
    return webrtc::WrapI420Buffer(width_, height_, dst_y, stride_y, dst_u, stride_u, dst_v, stride_v, unused);
  }

  return nullptr;
}

static __forceinline int toIndex(int x, int y, int width) {
  return x + y * width;
};

rtc::scoped_refptr<webrtc::I420BufferInterface>
D3D11VideoFrameBuffer::ToI420AlphaDepth(uint8_t* buffer, int capacity) {
  RTC_CHECK(buffer != nullptr);

  const int frame_width = subresource_width_;
  const int frame_height = subresource_height_;

  const int& width_y = frame_width;
  const int& height_y = frame_height;
  const int& stride_y = width_y;
  const int size_y = width_y * height_y;

  const int width_u = div_ceiled_fast(width_y, 2);
  const int height_u = div_ceiled_fast(height_y, 2);
  const int stride_u = width_u;
  const int size_u = width_u * height_u;

  // const int& width_v = width_u;
  // const int& height_v = height_u;
  const int& stride_v = stride_u;
  const int& size_v = size_u;
  const int size_yuv = size_y + size_u + size_v;

  const int has_depth_alpha = (int)(depth_texture_.get() != nullptr);
  RTC_CHECK(capacity >= size_yuv * (1 + has_depth_alpha));

  uint8_t* dst_y  = buffer;
  uint8_t* dst_dh = dst_y + (size_y * has_depth_alpha);

  uint8_t* dst_u  = dst_dh + size_y;
  uint8_t* dst_dl = dst_u + (size_u * has_depth_alpha);
  uint8_t* dst_v  = dst_dl + size_u;
  uint8_t* dst_a  = dst_v + (size_v * has_depth_alpha);

  auto convertToI420Alpha = [&](void* src_data, int src_row_pitch, int /* src_depth_pitch */) {
    if (!(color_texture_desc_.Format == DXGI_FORMAT_R8G8B8A8_UNORM
       || color_texture_desc_.Format == DXGI_FORMAT_R8G8B8A8_TYPELESS)) {
      FATAL() << "Unsupported color texture format";
      return;
    }

    uint8_t* src_data8 = static_cast<uint8_t*>(src_data);
    int conversion_result = libyuv::ARGBToI420(
        src_data8, src_row_pitch,
        OUT dst_y, stride_y,
        OUT dst_u, stride_u,
        OUT dst_v, stride_v,
        frame_width, frame_height);

    if (conversion_result != 0) {
      RTC_LOG(LS_ERROR) << "i420 conversion failed with error code" << conversion_result;
      // crash in debug mode so we can find out what went wrong
      RTC_DCHECK(conversion_result == 0);
    }

    if (depth_texture_.get() != nullptr) {
      // Alpha: pack stuff into V plane of depth data to test if it works
      // very unfortunate to loop over this same data twice :/
      // maybe use ARGBToUVRow instead of this at some point (not public api :/ but CopyPlane is, maybe useful)

#ifdef viktor
      // uint32_t width_in_pixels = src_row_pitch / 4;  // 4 because 4 components, 8-bit each
      // uint32_t uv_write_index = (width_ / 2 * height_ / 2) / 2;  // same as below
      // libyuv::CopyPlane(src_data8+3, src_row_pitch, dst_a, size_u, stride_u, height_u);

      // skip every second value in every direction
      // TODO: apply filter e.g. bicubic instead of throwing away values
      constexpr int rgba_width = 4;
      int dst_i = 0;
      // for (int src_y = 0; src_y < frame_height; src_y += 1 * 2) {
      //   for (int src_x = 3 /* rgb_a_ */; src_x < frame_width * rgba_width; src_x += rgba_width /* argb */ * 2) {
      //     // if ((src_y % 2 == 0) && (src_x % 2 == 0)) {
      //       // maybe instead of taking the raw value of this pixel, take the average or something.
      //       // https://docs.microsoft.com/en-us/windows/win32/api/dxgiformat/ne-dxgiformat-dxgi_format#portable-coding-for-endian-independence
      //       dst_a[dst_i++] = src_data8[toIndex(src_x, src_y, frame_width * rgba_width)];
      //     // }
      //   }
      // }

      for (int src_y = 0; src_y < frame_height; src_y += 2)
      for (int src_x = 0; src_x < frame_width;  src_x += 2) {
          // if ((src_y % 2 == 0) && (src_x % 2 == 0)) {
            // maybe instead of taking the raw value of this pixel, take the average or something.
            // https://docs.microsoft.com/en-us/windows/win32/api/dxgiformat/ne-dxgiformat-dxgi_format#portable-coding-for-endian-independence
            int alpha_sum = 0;
            alpha_sum = src_data8[toIndex( src_x      * rgba_width + 3, src_y    , frame_width * rgba_width)];
            alpha_sum = src_data8[toIndex((src_x + 1) * rgba_width + 3, src_y    , frame_width * rgba_width)];
            alpha_sum = src_data8[toIndex( src_x      * rgba_width + 3, src_y + 1, frame_width * rgba_width)];
            alpha_sum = src_data8[toIndex((src_x + 1) * rgba_width + 3, src_y + 1, frame_width * rgba_width)];
            dst_a[dst_i++] = alpha_sum / 4;
          // }
      }

      // cpu avg filter attempt
      // {
      //   auto toIndex = [](int x, int y, int frame_width) {
      //     return x+y*frame_width;
      //   };

      //   int src_row_pitch = 4, dst_width = 2;
      //   int dst_x = 0, dst_y = 0;

      //   constexpr int ratio = 2 /* src_row_pitch / dst_width */;

      //   memset(dst_a, 0, size_u);
      //   for (int src_y = 0; src_y < frame_height; src_y++)
      //   for (int src_x = 0; src_x < frame_width;  src_x++)
      //   {
      //     dst_x = div_ceiled_fast(src_x, ratio);
      //     dst_y = div_ceiled_fast(src_y, ratio);
      //     dst_a[toIndex(dst_x, dst_y, dst_width)] += src_data8[toIndex(src_x, src_y, frame_width)];
      //   }

      //   for (size_t i = 0; i < size_u; i++)
      //   {
      //     dst_a[i] /= 4;
      //   }
      // }
#else // krzysztof
      size_t dst_i = 0;
      size_t x_write = 0, y_write = 0;
#ifdef ISAR_BRANCHLESS
      memset(dst_a, 0, size_v);
#endif
      for (size_t y = 0; y < (size_t)frame_height; y++) {
        for (size_t x = 0; x < (size_t)frame_width; x++) {

#ifndef ISAR_BRANCHLESS
            // with branch
          if (x_write && y_write) {
            // maybe instead of taking the raw value of this pixel, take the
            // average or something.
            // https://docs.microsoft.com/en-us/windows/win32/api/dxgiformat/ne-dxgiformat-dxgi_format#portable-coding-for-endian-independence
            dst_a[dst_i] = src_data8[3];
            dst_i++;
          }
#else
            // branchless (significantly worse! branch predictor handles it pretty well)
            size_t should_write = x_write * y_write;
            dst_a[dst_i] += src_data8[3] * should_write;
            dst_i += should_write;
#endif

          // 1 pixel = 4 bytes for RGBA
          src_data8 += 4;
          x_write = 1 - x_write;
        }
        y_write = 1 - y_write;
      }
#endif

    }
  };

  auto convertToDepth = [&](void* src_data, int src_row_pitch, int /* src_depth_pitch */) {
    if (!(depth_texture_desc_.Format == DXGI_FORMAT_R16_TYPELESS)) {
      FATAL() << "Unsupported depth texture format";
      return;
    }

    // int src_width_depth = src_row_pitch / 2;
    // RowPitch can be higher than width_, which breaks things (out of bounds writes).
    // Note that this could theoretically send the wrong image
    // i.e. if there's padding in the front instead of in the back of a row of pixels.

    // libyuv::CopyPlane(src_data8, src_row_pitch, dst_dh, size_u, stride_u, height_u);
    uint16_t* src_data16 = reinterpret_cast<uint16_t*>(src_data);
    // for (int i = 0; i < size_y; i++) {
    //   dst_dh[i] = src_data16[i] >> 8;
    // }

#ifdef viktor
    uint8_t* src_data8 = reinterpret_cast<uint8_t*>(src_data);
    constexpr int r16_width = 2;
    // int dst_i = 0;
    // for (int src_y = 0; src_y < frame_height; src_y += 1 * 2) {
    //   for (int src_x = 1 /* dh _dl_ */; src_x < frame_width * r16_width; src_x += r16_width /* dh dl */ * 2) {
    //     // maybe instead of taking the raw value of this pixel, take the average or something.
    //     // https://docs.microsoft.com/en-us/windows/win32/api/dxgiformat/ne-dxgiformat-dxgi_format#portable-coding-for-endian-independence
    //     dst_dl[dst_i] = src_data8[toIndex(src_x, src_y, frame_width * r16_width)];
    //     dst_i++;
    //   }
    // }
    int i = 0;
    for (int src_y = 0; src_y < frame_height; src_y+=2)
    for (int src_x = 0; src_x < frame_width;  src_x+=2) {
      // TODO: 16x2 -> 8x1 like libyuv::ARGBToUVRow_SSSE3
      dst_dh[toIndex(src_x    , src_y    , frame_width)] = src_data8[toIndex( src_x      * r16_width, src_y    , frame_width * r16_width)];
      dst_dh[toIndex(src_x + 1, src_y    , frame_width)] = src_data8[toIndex((src_x + 1) * r16_width, src_y    , frame_width * r16_width)];
      dst_dh[toIndex(src_x    , src_y + 1, frame_width)] = src_data8[toIndex( src_x      * r16_width, src_y + 1, frame_width * r16_width)];
      dst_dh[toIndex(src_x + 1, src_y + 1, frame_width)] = src_data8[toIndex((src_x + 1) * r16_width, src_y + 1, frame_width * r16_width)];

      int low_byte_offset = 1;
      unsigned dl_sum = 0;
      assert(toIndex( src_x      * r16_width + low_byte_offset, src_y    , frame_width * r16_width) == toIndex( src_x      * r16_width + low_byte_offset, src_y    , frame_width * r16_width) + low_byte_offset);
      dl_sum += src_data8[toIndex( src_x      * r16_width + low_byte_offset, src_y    , frame_width * r16_width)];
      dl_sum += src_data8[toIndex((src_x + 1) * r16_width + low_byte_offset, src_y    , frame_width * r16_width)];
      dl_sum += src_data8[toIndex( src_x      * r16_width + low_byte_offset, src_y + 1, frame_width * r16_width)];
      dl_sum += src_data8[toIndex((src_x + 1) * r16_width + low_byte_offset, src_y + 1, frame_width * r16_width)];
      dst_dl[i++] = dl_sum / 4;
    }

    // for (int y = 0; y < height_ / 2; y++) {
    //   for (uint32_t x = 0; x < static_cast<uint32_t>(width_); x++) {
    //     uint8_t high = *pixel_ptr >> 8;
    //     uint8_t low = static_cast<uint8_t>(*pixel_ptr);

    //     // Write high 8 bits into Y plane
    //     int32_t y_offset = width_ * height_ / 2;
    //     dst_y_[y * width_ + x + y_offset] = high;

    //     if ((y % 2 == 0) && (x % 2 == 0)) {
    //       // Low 8 bits go into U plane
    //       dst_u_[uv_write_index] = low;
    //       uv_write_index++;
    //     }

    //     pixel_ptr++;
    //   }
    // }
#else // krzysztof
#ifndef ISAR_PRODUCTION
        LARGE_INTEGER startingTime, endingTime, elapsedMicroseconds;
        LARGE_INTEGER frequency;
        static LONGLONG mean = 0; // running mean/avg: https://nullbuffer.com/articles/welford_algorithm.html
        static int i = 1;
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&startingTime);
#endif

        size_t dst_i = 0;
        size_t x_write = 1, y_write = 1;
#ifdef ISAR_BRANCHLESS
        memset(dst_dl, 0, size_u);
#endif
        // RowPitch can be higher than width_, which breaks things (out of
        // bounds writes). Note that this could theoretically send the wrong
        // image if there's i.e. padding in the front instead of in the back
        // of a row of pixels.
        for (size_t y = 0; y < (size_t)frame_height; y++) {
          for (size_t x = 0; x < (size_t)frame_width; x++) {
            uint8_t high = *src_data16 >> 8;
            uint8_t low = static_cast<uint8_t>(*src_data16);

            // Write high 8 bits into Y plane
            dst_dh[y * (size_t)frame_width + x] = high;

#ifndef ISAR_BRANCHLESS
            // with branch
            if (x_write && y_write) {
              // Low 8 bits go into U plane
              dst_dl[dst_i] = low;
              dst_i++;
            }
#else
            // branchless (significantly worse! branch predictor handles it pretty well)
            size_t should_write = x_write * y_write;
            dst_dl[dst_i] += low * should_write;
            dst_i += should_write;
#endif

            src_data16++;
            x_write = 1 - x_write;
          }
          y_write = 1 - y_write;
        }
#endif
#ifndef ISAR_PRODUCTION
        QueryPerformanceCounter(&endingTime);
        elapsedMicroseconds.QuadPart = endingTime.QuadPart - startingTime.QuadPart;
        elapsedMicroseconds.QuadPart *= 1000000;
        elapsedMicroseconds.QuadPart /= frequency.QuadPart;
        mean += (elapsedMicroseconds.QuadPart - mean) / i;
        if (i == 1000 || i == 1500 || i == 2000 || i == 5000) {
          RTC_LOG(LS_WARNING) << "Sample: " << i << " took " << elapsedMicroseconds.QuadPart << " microseconds. Mean: " << mean;
        }
        i++;
#endif
  };

  StageTextures();

  HRESULT hr = S_OK;
  ON_SUCCEEDED(UseStagedResource(color_staging_texture_.get(), convertToI420Alpha));
  if(depth_texture_.get())
    ON_SUCCEEDED(UseStagedResource(depth_staging_texture_.get(), convertToDepth));
  if(SUCCEEDED(hr))
  {
    rtc::Callback0<void> unused;
    // TODO: not needed, see ToNV12
    // webrtc::I420BufferPool
    return webrtc::WrapI420Buffer(width_, height_, dst_y, stride_y, dst_u, stride_u, dst_v, stride_v, unused);
  }

  return nullptr;
}

// Stages, downloads and converts texture to NV12
uint8_t* D3D11VideoFrameBuffer::ToNV12(uint8_t* buffer, int capacity) {
  // TODO: redo to match ToI420
  __debugbreak();
  return nullptr;

  RTC_CHECK(buffer != nullptr);
  int frame_width = width_, frame_height = height_;
  int stride_y = frame_width;
  int size_y = frame_width * frame_height;
  int stride_uv = div_ceiled_fast(frame_width, 2);
  int size_u = stride_uv * div_ceiled_fast(frame_height, 2);
  RTC_CHECK(capacity >= size_y + 2 * size_u);

  StageTextures();

  D3D11_MAPPED_SUBRESOURCE mapped;
  HRESULT hr = context_->Map(color_staging_texture_.get(), 0, D3D11_MAP_READ, 0, &mapped);

  if (SUCCEEDED(hr)) {
    // int frame_width = width_, frame_height = height_;
    // int stride_y = frame_width;
    // int size_y = frame_width * frame_height;
    // int stride_uv = div_ceiled_fast(frame_width, 2);
    // int size_u = stride_uv * div_ceiled_fast(frame_height, 2);
    // uint8_t* dst_y = buffer;
    // uint8_t* dst_uv = buffer + size_y;
    // uint8_t* dst_v = dst_u + size_u;

    if (color_texture_desc_.Format == DXGI_FORMAT_R8G8B8A8_UNORM
     || color_texture_desc_.Format == DXGI_FORMAT_R8G8B8A8_TYPELESS) {
      int32_t conversion_result = libyuv::ARGBToNV12(
          static_cast<uint8_t*>(mapped.pData), mapped.RowPitch,
          OUT buffer, stride_y,
          OUT buffer + size_y, stride_uv,
          frame_width, frame_height);

      if (conversion_result != 0) {
        RTC_LOG(LS_ERROR) << "nv12 conversion failed with error code" << conversion_result;
        // crash in debug mode so we can find out what went wrong
        RTC_DCHECK(conversion_result == 0);
      }
    } else {
      FATAL() << "Unsupported texture format";
    }

    context_->Unmap(color_staging_texture_.get(), 0);

    return buffer;
    // rtc::Callback0<void> unused;
    // return webrtc::WrapI420Buffer(width_, height_, dst_y, stride_y, dst_u, stride_uv, dst_v, stride_uv, unused);
  }

  return nullptr;
}

uint8_t* D3D11VideoFrameBuffer::ToNV12AlphaDepth(uint8_t* buffer, int capacity) {
  // TODO: redo to match ToI420
  __debugbreak();
  return nullptr;

  RTC_CHECK(buffer != nullptr);
  const int has_depth_alpha = (int)(depth_texture_.get() != nullptr);

  const int frame_width = width_;
  const int frame_height = height_ / (1 + has_depth_alpha);

  const int& width_y = frame_width;
  const int& height_y = frame_height;
  const int& stride_y = width_y;
  const int size_y = width_y * height_y;

  const int width_u = div_ceiled_fast(width_y, 2);
  const int height_u = div_ceiled_fast(height_y, 2);
  const int stride_u = width_u;
  const int size_u = width_u * height_u;

  // const int& width_v = width_u;
  // const int& height_v = height_u;
  // const int& stride_v = stride_u;
  const int& size_v = size_u;
  const int size_yuv = size_y + size_u + size_v;
  RTC_CHECK(capacity >= size_yuv * (1 + has_depth_alpha));

  uint8_t* dst_y  = buffer;
  uint8_t* dst_dh = dst_y + size_y;

  uint8_t* dst_u  = dst_dh + size_y;
  uint8_t* dst_v  = dst_u + size_u;
  uint8_t* dst_dl = dst_v + size_v;
  uint8_t* dst_a  = dst_dl + size_u;

  auto convertToNV12Alpha = [&](void* src_data, int src_row_pitch, int /* src_depth_pitch */) {
    if (!(color_texture_desc_.Format == DXGI_FORMAT_R8G8B8A8_UNORM
       || color_texture_desc_.Format == DXGI_FORMAT_R8G8B8A8_TYPELESS)) {
      FATAL() << "Unsupported color texture format";
      return;
    }

    uint8_t* src_data8 = static_cast<uint8_t*>(src_data);
    int32_t conversion_result = libyuv::ARGBToNV12(
        src_data8, src_row_pitch,
        OUT dst_y, stride_y,
        OUT dst_u, stride_u,
        frame_width, frame_height);

    if (conversion_result != 0) {
      RTC_LOG(LS_ERROR) << "i420 conversion failed with error code" << conversion_result;
      // crash in debug mode so we can find out what went wrong
      RTC_DCHECK(conversion_result == 0);
    }

    if (depth_texture_.get() != nullptr) {
      // Alpha: pack stuff into V plane of depth data to test if it works
      // very unfortunate to loop over this same data twice :/
      // maybe use ARGBToUVRow instead of this at some point (not public api :/ but CopyPlane is, maybe useful)

      // uint32_t width_in_pixels = src_row_pitch / 4;  // 4 because 4 components, 8-bit each
      // uint32_t uv_write_index = (width_ / 2 * height_ / 2) / 2;  // same as below
      // libyuv::CopyPlane(src_data8+3, src_row_pitch, dst_a, size_u, stride_u, height_u);

      // skip every second value in every direction
      // TODO: apply filter e.g. bicubic instead of throwing away values
      int dst_i = 0;
      for (int src_y = 0; src_y < frame_height; src_y += 1 * 2) {
        for (int src_x = 3 /* rgb_a_ */; src_x < frame_width; src_x += 4 /* argb */ * 2) {
          // if ((src_y % 2 == 0) && (src_x % 2 == 0)) {
            // maybe instead of taking the raw value of this pixel, take the average or something.
            // https://docs.microsoft.com/en-us/windows/win32/api/dxgiformat/ne-dxgiformat-dxgi_format#portable-coding-for-endian-independence
            dst_a[dst_i++] = src_data8[toIndex(src_x, src_y, frame_width)];
          // }
        }
      }

      // cpu avg filter attempt
      // {
      //   int src_row_pitch = 4, dst_width = 2;
      //   int dst_x = 0, dst_y = 0;

      //   constexpr int ratio = 2 /* src_row_pitch / dst_width */;

      //   memset(dst_a, 0, size_u);
      //   for (int src_y = 0; src_y < frame_height; src_y++)
      //   for (int src_x = 0; src_x < frame_width;  src_x++)
      //   {
      //     dst_x = div_ceiled_fast(src_x, ratio);
      //     dst_y = div_ceiled_fast(src_y, ratio);
      //     dst_a[toIndex(dst_x, dst_y, dst_width)] += src_data8[toIndex(src_x, src_y, frame_width)];
      //   }

      //   for (size_t i = 0; i < size_u; i++)
      //   {
      //     dst_a[i] /= 4;
      //   }
      // }

    }
  };

  auto convertToDepth = [&](void* src_data, int src_row_pitch, int /* src_depth_pitch */) {
    if (!(depth_texture_desc_.Format == DXGI_FORMAT_R16_TYPELESS)) {
      FATAL() << "Unsupported depth texture format";
      return;
    }

    // int src_width_depth = src_row_pitch / 2;
    // RowPitch can be higher than width_, which breaks things (out of bounds writes).
    // Note that this could theoretically send the wrong image
    // i.e. if there's padding in the front instead of in the back of a row of pixels.

    // libyuv::CopyPlane(src_data8, src_row_pitch, dst_dh, size_u, stride_u, height_u);
    uint16_t* src_data16 = reinterpret_cast<uint16_t*>(src_data);
    for (int i = 0; i < size_y; i++) {
      dst_dh[i] = src_data16[i] >> 8;
    }

    uint8_t* src_data8 = reinterpret_cast<uint8_t*>(src_data);
    int dst_i = 0;
    for (int src_y = 0; src_y < frame_height; src_y += 1 * 2) {
      for (int src_x = 1 /* dh _dl_ */; src_x < frame_width; src_x += 2 /* dh dl */ * 2) {
        // maybe instead of taking the raw value of this pixel, take the average or something.
        // https://docs.microsoft.com/en-us/windows/win32/api/dxgiformat/ne-dxgiformat-dxgi_format#portable-coding-for-endian-independence
        dst_dl[dst_i] = src_data8[toIndex(src_x, src_y, frame_width)];
        dst_i++;
      }
    }

    // for (int y = 0; y < height_ / 2; y++) {
    //   for (uint32_t x = 0; x < static_cast<uint32_t>(width_); x++) {
    //     uint8_t high = *pixel_ptr >> 8;
    //     uint8_t low = static_cast<uint8_t>(*pixel_ptr);

    //     // Write high 8 bits into Y plane
    //     int32_t y_offset = width_ * height_ / 2;
    //     dst_y_[y * width_ + x + y_offset] = high;

    //     if ((y % 2 == 0) && (x % 2 == 0)) {
    //       // Low 8 bits go into U plane
    //       dst_u_[uv_write_index] = low;
    //       uv_write_index++;
    //     }

    //     pixel_ptr++;
    //   }
    // }
  };

  StageTextures();

  HRESULT hr = S_OK;
  ON_SUCCEEDED(UseStagedResource(color_staging_texture_.get(), convertToNV12Alpha));
  if(depth_texture_.get())
    ON_SUCCEEDED(UseStagedResource(depth_staging_texture_.get(), convertToDepth));
  if(SUCCEEDED(hr))
  {
    return buffer;
  }

  return nullptr;
}

// Stages, downloads and converts texture to NV12
uint8_t* D3D11VideoFrameBuffer::ToNV12_New(uint8_t* buffer, int capacity) {
  // TODO: redo to match ToI420
  __debugbreak();
  return nullptr;

  RTC_CHECK(buffer != nullptr);
  int frame_width = width_, frame_height = height_;
  int stride_y = frame_width;
  int size_y = frame_width * frame_height;
  int stride_uv = div_ceiled_fast(frame_width, 2);
  int size_u = stride_uv * div_ceiled_fast(frame_height, 2);
  RTC_CHECK(capacity >= size_y + 2 * size_u);

  auto convertToNV12 = [&](void* src_data, int src_row_pitch, int /* src_depth_pitch */) {
    if (color_texture_desc_.Format == DXGI_FORMAT_R8G8B8A8_UNORM
     || color_texture_desc_.Format == DXGI_FORMAT_R8G8B8A8_TYPELESS) {
      int32_t conversion_result = libyuv::ARGBToNV12(
          static_cast<uint8_t*>(src_data), src_row_pitch,
          OUT buffer, stride_y,
          OUT buffer + size_y, stride_uv,
          frame_width, frame_height);

      if (conversion_result != 0) {
        RTC_LOG(LS_ERROR) << "nv12 conversion failed with error code" << conversion_result;
        // crash in debug mode so we can find out what went wrong
        RTC_DCHECK(conversion_result == 0);
      }
    } else {
      FATAL() << "Unsupported texture format";
    }
  };

  StageTextures();

  if(SUCCEEDED(UseStagedResource(color_staging_texture_.get(), convertToNV12)))
  {
    return buffer;
  }
  return nullptr;
}

// Copies contents of color_texture_ to color_staging_texture_ and downloads it to
// the CPU.
// uint8_t* D3D11VideoFrameBuffer::LoadToMemory(uint8_t* OUT destination, size_t capacity)
// {
//   RTC_CHECK(destination != nullptr);
//   int frame_width = width_, frame_height = height_;
//   int stride_y = frame_width;
//   int size_y = frame_width * frame_height;
//   int stride_uv = div_ceiled_fast(frame_width, 2);
//   int size_u = stride_uv * div_ceiled_fast(frame_height, 2);
//   RTC_CHECK(capacity >= size_y + 2 * size_u);
// }

// Stages the GPU texture for usage in CPU memory
void D3D11VideoFrameBuffer::StageTextures() {
  // TODO: Last arg should use the number of mip levels from color_texture_,
  // which I guess is part of its description. Also, we need 2
  // CopySubresourceRegion calls that use the dimensions of the texture....so we need desc again?
  // D3D11_TEXTURE2D_DESC color_texture_desc = {};
  // color_texture_->GetDesc(&color_texture_desc);

  D3D11_BOX region{0, 0, 0, subresource_width_, subresource_height_, 1};

  auto *depth_ptr = depth_texture_.get();
  if (color_texture_desc_.ArraySize == 1) {
    // single double-wide texture
    context_->CopySubresourceRegion(color_staging_texture_.get(), 0, 0, 0, 0, color_texture_.get(), subresource_index_, &region);

    if (depth_ptr != nullptr) {
      context_->CopyResource(depth_staging_texture_.get(), depth_texture_.get());
    }

  } else if (color_texture_desc_.ArraySize == 2) {
    // texture array (2 images)
    UINT left_eye_color_subresource = D3D11CalcSubresource(0, 0, color_texture_desc_.MipLevels);
    UINT right_eye_color_subresource = D3D11CalcSubresource(0, 1, color_texture_desc_.MipLevels);

    context_->CopySubresourceRegion(color_staging_texture_.get(), 0, 0, 0, 0, color_texture_.get(), left_eye_color_subresource, 0);
    context_->CopySubresourceRegion(color_staging_texture_.get(), 0, color_texture_desc_.Width, 0, 0, color_texture_.get(), right_eye_color_subresource, 0);

    if (depth_ptr != nullptr) {
      // for single pass copy to staging texture array first, then copy to double-wide/high texture
      UINT left_eye_depth_subresource = D3D11CalcSubresource(0, 0, depth_texture_desc_.MipLevels);
      UINT right_eye_depth_subresource = D3D11CalcSubresource(0, 1, depth_texture_desc_.MipLevels);
#if true
      context_->CopyResource(depth_staging_texture_array_.get(), depth_texture_.get());
      context_->CopySubresourceRegion(depth_staging_texture_.get(), 0, 0, 0, 0, depth_staging_texture_array_.get(), left_eye_depth_subresource, 0);
      context_->CopySubresourceRegion(depth_staging_texture_.get(), 0, color_texture_desc_.Width, 0, 0, depth_staging_texture_array_.get(), right_eye_depth_subresource, 0);
#else
      context_->CopySubresourceRegion(depth_staging_texture_.get(), 0, 0, 0, 0, depth_texture_.get(), left_eye_depth_subresource, 0);
      context_->CopySubresourceRegion(depth_staging_texture_.get(), 0, color_texture_desc_.Width, 0, 0, depth_texture_.get(), right_eye_depth_subresource, 0);
#endif
    }

  } else {
    RTC_LOG(LS_WARNING) << "Got frame with ArraySize > 2, not sure what to do with this";
    // send black frame maybe? I think there were methods for that.
    // or we use the subresource index that has been set in the decoder (in case
    // of client), copy that to staging and whatever. Then again, if the decoder
    // allocates just 2 textures we treat them as eyes even though they're not.
    // Not sure what the best solution is here.
  }

}

template<typename T>
HRESULT D3D11VideoFrameBuffer::UseStagedResource(ID3D11Resource *resource, T/*void T(void*data, int stride)*/ action) {
  D3D11_MAPPED_SUBRESOURCE mapped = {};
  HRESULT hr = context_->Map(resource, 0, D3D11_MAP_READ, 0, &mapped);
  if (SUCCEEDED(hr)) {
    action(mapped.pData, mapped.RowPitch, mapped.DepthPitch);
    context_->Unmap(resource, 0);
  }
  return hr;
}

}  // namespace hlr
