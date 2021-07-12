/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/logging.h"
#include "third_party/libyuv/include/libyuv/convert.h"

#include "d3d11_frame_buffer.h"
#include "d3d11_frame_source.h"

using webrtc::VideoFrame;

namespace hlr {
rtc::scoped_refptr<D3D11VideoFrameSource> D3D11VideoFrameSource::Create(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    D3D11_TEXTURE2D_DESC* color_desc,
    D3D11_TEXTURE2D_DESC* depth_desc,
    rtc::Thread* signaling_thread) {
  return new rtc::RefCountedObject<D3D11VideoFrameSource>(
      device, context, color_desc, depth_desc, signaling_thread);
}

D3D11VideoFrameSource::D3D11VideoFrameSource(ID3D11Device* device,
                                             ID3D11DeviceContext* context,
                                             D3D11_TEXTURE2D_DESC* color_desc,
                                             D3D11_TEXTURE2D_DESC* depth_desc,
                                             rtc::Thread* signaling_thread)
    : signaling_thread_(signaling_thread), is_screencast_(false) {
  assert(device);
  assert(context);
  assert(color_desc);

  device_.copy_from(device);
  context_.copy_from(context);

  DXGI_SAMPLE_DESC sample_desc;
  sample_desc.Count = 1;
  sample_desc.Quality = 0;

  D3D11_TEXTURE2D_DESC staging_desc = {};
  staging_desc.ArraySize = 1;
  staging_desc.BindFlags = 0;
  staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  staging_desc.Format = color_desc->Format;
  staging_desc.Height = color_desc->Height;
  staging_desc.MipLevels = color_desc->MipLevels;
  staging_desc.MiscFlags = color_desc->MiscFlags;
  staging_desc.SampleDesc = sample_desc;
  // We want to read back immediately after copying, hence staging.
  staging_desc.Usage = D3D11_USAGE_STAGING;

  // multiply by 2 if desc->arraysize is 2...or multiply by arraysize. Wait, no,
  // ArraySize can be 0.
  bool single_pass = color_desc->ArraySize == 2;
  if (single_pass) {
    width_ = staging_desc.Width = color_desc->Width * 2;
  } else {
    // Multi-pass already gives us double-wide, so we use the width as is.
    width_ = staging_desc.Width = color_desc->Width;
  }

  if (depth_desc != nullptr) {
    // Double the height because we have depth information.
    height_ = color_desc->Height * 2;
  } else {
    // Use existing height if there's no depth info.
    height_ = color_desc->Height;
  }

  // U and V are always half on each side, so 1/4 of the Y plane size.
  dst_y_ = static_cast<uint8_t*>(malloc(width_ * height_));
  dst_u_ = static_cast<uint8_t*>(malloc((width_ / 2) * (height_ / 2)));
  dst_v_ = static_cast<uint8_t*>(malloc((width_ / 2) * (height_ / 2)));

  HRESULT hr =
      device_->CreateTexture2D(&staging_desc, nullptr, staging_texture_.put());

  std::string name = "D3D11VideoFrameSource_Staging";
  staging_texture_->SetPrivateData(WKPDID_D3DDebugObjectName, name.length(),
                                   name.c_str());

  if (FAILED(hr)) {
    // TODO: something sensible, but constructors can't return values...meh.
    // maybe we should write a static Create method that can fail instead.
    RTC_LOG_GLE_EX(LS_ERROR, hr) << "Failed creating the staging texture. The "
                                    "D3D11 frame source will not work.";
  }

  // If we get a desc for depth, allocate a staging texture for depth
  if (depth_desc != nullptr) {
    staging_desc.Format = depth_desc->Format;

    hr = device_->CreateTexture2D(&staging_desc, nullptr,
                                  depth_staging_texture_.put());

    if (FAILED(hr)) {
      RTC_LOG_GLE_EX(LS_ERROR, hr)
          << "Failed creating the depth staging texture. The "
             "D3D11 frame source will not work.";
    }
    name = "D3D11VideoFrameSource_Depth_Staging";
    depth_staging_texture_->SetPrivateData(WKPDID_D3DDebugObjectName,
                                           name.length(), name.c_str());

    // Another special case: you can't copy part of a resource that is
    // BIND_DEPTH_STENCIL (which depth buffers are). That means we can't copy
    // left and right eyes separately to a staging texture; we can only copy the
    // whole thing at once, which means our staging texture for depth needs to
    // be an array, just like Unity's depth texture is.
    if (single_pass) {
      staging_desc.ArraySize = 2;
      staging_desc.Width = depth_desc->Width;

      hr = device_->CreateTexture2D(&staging_desc, nullptr,
                                    depth_staging_texture_array_.put());
      if (FAILED(hr)) {
        RTC_LOG_GLE_EX(LS_ERROR, hr)
            << "Failed creating the depth staging texture array. The "
               "D3D11 frame source will not work.";
      }
      name = "D3D11VideoFrameSource_Depth_Staging_Array";
      depth_staging_texture_array_->SetPrivateData(WKPDID_D3DDebugObjectName,
                                                   name.length(), name.c_str());
    }
  }

  // not sure if we need to notify...but maybe we could call setstate from the
  // outside.
  state_ = webrtc::MediaSourceInterface::SourceState::kLive;
}

void D3D11VideoFrameSource::SetState(
    rtc::AdaptedVideoTrackSource::SourceState state) {
  if (rtc::Thread::Current() != signaling_thread_) {
    invoker_.AsyncInvoke<void>(
        RTC_FROM_HERE, signaling_thread_,
        rtc::Bind(&D3D11VideoFrameSource::SetState, this, state));
    return;
  }

  if (state != state_) {
    state_ = state;
    FireOnChanged();
  }
}

D3D11VideoFrameSource::~D3D11VideoFrameSource() {
  if (dst_y_ != nullptr) {
    free(dst_y_);
  }

  if (dst_u_ != nullptr) {
    free(dst_u_);
  }

  if (dst_v_ != nullptr) {
    free(dst_v_);
  }
}

void D3D11VideoFrameSource::OnFrameCaptured(ID3D11Texture2D* rendered_image,
                                            ID3D11Texture2D* depth_image,
                                            webrtc::XRTimestamp timestamp) {
  int64_t time_us = rtc::TimeMicros();

  int adapted_width;
  int adapted_height;
  int crop_width;
  int crop_height;
  int crop_x;
  int crop_y;

  if (!AdaptFrame(width_, height_, time_us, &adapted_width, &adapted_height,
                  &crop_width, &crop_height, &crop_x, &crop_y)) {
    return;
  }

  D3D11_TEXTURE2D_DESC desc = {};
  rendered_image->GetDesc(&desc);

  auto d3d_frame_buffer = D3D11VideoFrameBuffer::Create(
      context_.get(), staging_texture_.get(), rendered_image,
      depth_staging_texture_.get(), depth_staging_texture_array_.get(),
      depth_image, dst_y_, dst_u_, dst_v_, desc, width_, height_);

  // on windows, the best way to do this would be to convert to nv12 directly
  // since the encoder expects that. libyuv features an ARGBToNV12 function. The
  // problem now is, which frame type do we use? There's none for nv12. I guess
  // we'd need to modify. Then we could also introduce d3d11 as frame type.
  auto i420_buffer = d3d_frame_buffer->ToI420();

  auto frame = VideoFrame::Builder()
                   .set_video_frame_buffer(i420_buffer)
                   .set_timestamp_us(time_us)
                   .build();
  frame.set_xr_timestamp(timestamp);
  OnFrame(frame);
}

absl::optional<bool> D3D11VideoFrameSource::needs_denoising() const {
  return false;
}

bool D3D11VideoFrameSource::is_screencast() const {
  return is_screencast_;
}

rtc::AdaptedVideoTrackSource::SourceState D3D11VideoFrameSource::state() const {
  return state_;
}

bool D3D11VideoFrameSource::remote() const {
  return false;
}
}  // namespace hlr
