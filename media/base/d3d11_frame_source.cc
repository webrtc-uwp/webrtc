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

namespace hololight {
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

  DXGI_SAMPLE_DESC sampleDesc;
  sampleDesc.Count = 1;
  sampleDesc.Quality = 0;

  // the braces make sure sure the struct is zero-initialized
  D3D11_TEXTURE2D_DESC stagingDesc = {};
  stagingDesc.ArraySize = desc->ArraySize;
  stagingDesc.BindFlags = 0;
  stagingDesc.CPUAccessFlags =
      D3D11_CPU_ACCESS_READ;  // for now read access should be enough
  stagingDesc.Format = desc->Format;
  stagingDesc.Height = desc->Height;
  stagingDesc.MipLevels = desc->MipLevels;
  stagingDesc.MiscFlags = desc->MiscFlags;
  stagingDesc.SampleDesc = sampleDesc;
  stagingDesc.Usage = D3D11_USAGE_STAGING;  // we want to read back immediately
                                            // after copying, hence staging.
  stagingDesc.Width = desc->Width;

  width_ = desc->Width;
  height_ = desc->Height;

  dst_y_ = static_cast<uint8_t*>(malloc(width_ * height_));
  dst_u_ = static_cast<uint8_t*>(malloc((width_ / 2) * (height_ / 2)));
  dst_v_ = static_cast<uint8_t*>(malloc((width_ / 2) * (height_ / 2)));

  // wait. can we even use exceptions in this lib? windows version has rtti
  // enabled via gn, not sure if this implies exceptions.
  // https://google.github.io/styleguide/cppguide.html#Windows_Code says
  // exceptions are ok in stl code, which is why they are enabled in windows
  // builds. still, we shouldn't write exception handling code ourselves...well,
  // I'm confused. Not even sure what to do for our own lib. Result<T, E> would
  // be nice. Or Rust.
  HRESULT hr =
      device_->CreateTexture2D(&stagingDesc, nullptr, staging_texture_.put());
  if (FAILED(hr)) {
    // TODO: something sensible, but constructors can't return values...meh.
    // maybe we should write a static Create method that can fail instead.
    RTC_LOG_GLE_EX(LS_ERROR, hr) << "Failed creating the staging texture. The "
                                    "D3D11 frame source will not work.";
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

  if (!AdaptFrame(width_, height_, time_us, &adapted_width, &adapted_height,
                  &crop_width, &crop_height, &crop_x, &crop_y)) {
    return;
  }

  auto d3dFrameBuffer = D3D11VideoFrameBuffer::Create(
      context_.get(), staging_texture_.get(), rendered_image, width_, height_,
      dst_y_, dst_u_, dst_v_);

  // on windows, the best way to do this would be to convert to nv12 directly
  // since the encoder expects that. libyuv features an ARGBToNV12 function. The
  // problem now is, which frame type do we use? There's none for nv12. I guess
  // we'd need to modify. Then we could also introduce d3d11 as frame type.
  auto i420Buffer = d3dFrameBuffer->ToI420();

  // TODO: AdaptFrame somewhere, probably needs an override to deal with d3d and
  // such
  // 5 months later: AdaptFrame doesn't modify the frame data at all. It just
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
}  // namespace hololight