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
    D3D11_TEXTURE2D_DESC* desc,
    rtc::Thread* signaling_thread) {
  return new rtc::RefCountedObject<D3D11VideoFrameSource>(device, context, desc,
                                                          signaling_thread);
}

D3D11VideoFrameSource::D3D11VideoFrameSource(ID3D11Device* device,
                                             ID3D11DeviceContext* context,
                                             D3D11_TEXTURE2D_DESC* desc,
                                             rtc::Thread* signaling_thread)
    : signaling_thread_(signaling_thread), is_screencast_(false) {
  assert(device);
  assert(context);
  assert(desc);

  device_.copy_from(device);
  context_.copy_from(context);

  DXGI_SAMPLE_DESC sample_desc;
  sample_desc.Count = 1;
  sample_desc.Quality = 0;

  D3D11_TEXTURE2D_DESC staging_desc = *desc;
  assert(staging_desc.ArraySize > 0);
  staging_desc.Width *= staging_desc.ArraySize; // copy all textures into a single texture
  staging_desc.ArraySize = 1;
  staging_desc.SampleDesc = DXGI_SAMPLE_DESC{ /*Count*/ 1, /*Quality*/ 0 };
  staging_desc.Usage = D3D11_USAGE_STAGING; // we want to read back immediately after copying, hence staging
  staging_desc.BindFlags = 0;
  staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ; // for now read access should be enough

  width_ = staging_desc.Width;
  height_ = staging_desc.Height;
  texture_format_ = staging_desc.Format;

  int dst_y_size = (width_ * height_);
  int dst_u_size = div_ceiled_fast(width_, 2) * div_ceiled_fast(height_, 2);
  frame_mem_arena_ = static_cast<uint8_t*>(malloc(dst_y_size + 2 * dst_u_size));

  // wait. can we even use exceptions in this lib? windows version has rtti
  // enabled via gn, not sure if this implies exceptions.
  // https://google.github.io/styleguide/cppguide.html#Windows_Code says
  // exceptions are ok in stl code, which is why they are enabled in windows
  // builds. still, we shouldn't write exception handling code ourselves...well,
  // I'm confused. Not even sure what to do for our own lib. Result<T, E> would
  // be nice. Or Rust.
  HRESULT hr = device_->CreateTexture2D(&staging_desc, nullptr, staging_texture_.put());

  std::string name = "D3D11VideoFrameSource_Staging";
  staging_texture_->SetPrivateData(WKPDID_D3DDebugObjectName, name.length(), name.c_str());

  RTC_CHECK(SUCCEEDED(hr)) << "Failed creating the staging texture. The D3D11 frame source will not work.";
  // if (FAILED(hr)) {
  //   // TODO: something sensible, but constructors can't return values...meh.
  //   // maybe we should write a static Create method that can fail instead.
  //   RTC_LOG_GLE_EX(LS_ERROR, hr) << "Failed creating the staging texture. The D3D11 frame source will not work.";
  // }

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
  free(frame_mem_arena_);
}

void D3D11VideoFrameSource::OnFrameCaptured(ID3D11Texture2D* rendered_image,
                                            webrtc::XRTimestamp timestamp) {
  // TODO: this should get its config from somewhere else
  // also, right now we should copy to staging, download to cpu,
  // convert with libyuv and then call OnFrame. I think that'd be the
  // easiest way to get something on screen. Later we can investigate leaving
  // the texture on the gpu until it's encoded.
  int64_t time_us = rtc::TimeMicros();

  int adapted_width;
  int adapted_height;
  int crop_width;
  int crop_height;
  int crop_x;
  int crop_y;

  // ok, so that's why we need the width of the whole frame, so webrtc knows
  // about it. we can get that from staging_desc, though
  if (!AdaptFrame(width_, height_, time_us, &adapted_width, &adapted_height,
                  &crop_width, &crop_height, &crop_x, &crop_y)) {
    return;
  }

  D3D11_TEXTURE2D_DESC desc = {};
  rendered_image->GetDesc(&desc);

  auto d3dFrameBuffer = D3D11VideoFrameBuffer::Create(
      context_.get(), staging_texture_.get(), rendered_image, desc);

  // on windows, the best way to do this would be to convert to nv12 directly
  // since the encoder expects that. libyuv features an ARGBToNV12 function. The
  // problem now is, which frame type do we use? There's none for nv12. I guess
  // we'd need to modify. Then we could also introduce d3d11 as frame type.
  int dst_y_size = (width_ * height_);
  int dst_u_size = div_ceiled_fast(width_, 2) * div_ceiled_fast(height_, 2);
  int size = dst_y_size + 2 * dst_u_size;
  auto i420Buffer = d3dFrameBuffer->ToI420(frame_mem_arena_, size);

  // TODO: AdaptFrame somewhere, probably needs an override to deal with d3d and such
  // NOTE: 5 months later: AdaptFrame doesn't modify the frame data at all. It just
  // tells us when to do shit, which is nice. Also prevents us from spamming the
  // sink (networking) when it can't handle more shit.

  // TODO: set more stuff if needed, such as ntp timestamp
  auto frame = VideoFrame::Builder()
                   .set_video_frame_buffer(i420Buffer)
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
