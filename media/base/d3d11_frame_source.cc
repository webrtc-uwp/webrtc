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
#include "third_party/winuwp_h264/Utils/Utils.h"

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

D3D11VideoFrameSource::D3D11VideoFrameSource(
    ID3D11Device* device,
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

  D3D11_TEXTURE2D_DESC staging_desc = *color_desc;
  assert(staging_desc.ArraySize > 0);
  // NOTE: mono textures can be part of a texture array pool so we check for exactly 2 to narrow it down but it's error prone
  bool single_pass = staging_desc.ArraySize == 2; // TODO: is this really single pass or did you mean mono/stereo?
  if (single_pass) {
    staging_desc.Width *= 2; // merge stereo textures into a single texture
  }
  // if (depth_desc != nullptr) staging_desc.Height *= 2; // double the height to store depth information.
  staging_desc.ArraySize = 1;
  staging_desc.SampleDesc = DXGI_SAMPLE_DESC{ /*Count*/ 1, /*Quality*/ 0 };
  staging_desc.Usage = D3D11_USAGE_STAGING; // we want to read back immediately after copying, hence staging
  staging_desc.BindFlags = 0;
  staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ; // for now read access should be enough

  width_ = staging_desc.Width;
  height_ = staging_desc.Height;

#define PASS_STRING_LITERAL(_string) strlen(_string), _string

  HRESULT hr = S_OK;
  ON_SUCCEEDED(device_->CreateTexture2D(&staging_desc, nullptr, color_staging_texture_.put()));
  RTC_CHECK(SUCCEEDED(hr)) << "Failed creating the staging texture. The D3D11 frame source will not work.";
  ON_SUCCEEDED(color_staging_texture_->SetPrivateData(WKPDID_D3DDebugObjectName, PASS_STRING_LITERAL("D3D11VideoFrameSource_Staging")));

  // If we get a desc for depth, allocate a staging texture for depth
  if (depth_desc != nullptr) {
    height_ *= 2;
    staging_desc.Format = depth_desc->Format;

    ON_SUCCEEDED(device_->CreateTexture2D(&staging_desc, nullptr, depth_staging_texture_.put()));
    ON_SUCCEEDED(depth_staging_texture_->SetPrivateData(WKPDID_D3DDebugObjectName, PASS_STRING_LITERAL("D3D11VideoFrameSource_Depth_Staging")));

    // Another special case: you can't copy part of a resource that is
    // BIND_DEPTH_STENCIL (which depth buffers are). That means we can't copy
    // left and right eyes separately to a staging texture; we can only copy the
    // whole thing at once, which means our staging texture for depth needs to be an array, just like Unity's depth texture is.
    // TODO: which thing? is there one texture for both eyes, is that one you mean?
    if (single_pass) {
      staging_desc.ArraySize = 2;
      staging_desc.Width = depth_desc->Width;
      ON_SUCCEEDED(device_->CreateTexture2D(&staging_desc, nullptr, depth_staging_texture_array_.put()));
      ON_SUCCEEDED(depth_staging_texture_array_->SetPrivateData(WKPDID_D3DDebugObjectName, PASS_STRING_LITERAL("D3D11VideoFrameSource_Depth_Staging_Array")));
    }
  }

#undef PASS_STRING_LITERAL

  // depth is accounted for in the height_ variable
  const int dst_y_size = width_ * height_; // TODO: what about the bits per channel?
  const int dst_u_size = div_ceiled_fast(width_, 2) * div_ceiled_fast(height_, 2);
  frame_mem_arena_size_ = dst_y_size + 2 * dst_u_size;
  frame_mem_arena_ = static_cast<uint8_t*>(malloc(frame_mem_arena_size_));

  // not sure if we need to notify...but maybe we could call setstate from the outside.
  state_ = webrtc::MediaSourceInterface::SourceState::kLive;
}

void D3D11VideoFrameSource::SetState(rtc::AdaptedVideoTrackSource::SourceState state) {
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
  free(frame_mem_arena_);
}

void D3D11VideoFrameSource::OnFrameCaptured(ID3D11Texture2D* color_texture,
                                            ID3D11Texture2D* depth_texture,
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

  // height_ includes depth
  auto d3d_frame_buffer = D3D11VideoFrameBuffer::Create(
      context_.get(),
      color_texture, color_staging_texture_.get(),
      depth_texture, depth_staging_texture_.get(), depth_staging_texture_array_.get()//,
      /* TODO: width_, height_ */ /* adapted_width, adapted_height */);

  // on windows, the best way to do this would be to convert to nv12 directly
  // since the encoder expects that. libyuv features an ARGBToNV12 function. The
  // problem now is, which frame type do we use? There's none for nv12. I guess
  // we'd need to modify. Then we could also introduce d3d11 as frame type.
  auto i420_buffer = d3d_frame_buffer->ToI420AlphaDepth(frame_mem_arena_, frame_mem_arena_size_);

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
