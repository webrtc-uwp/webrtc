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

  // the braces make sure the struct is zero-initialized
  D3D11_TEXTURE2D_DESC staging_desc = {};
  staging_desc.ArraySize = 1;
  staging_desc.BindFlags = 0;
  // for now read access should be enough
  staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  staging_desc.Format = desc->Format;
  staging_desc.Height = desc->Height;
  staging_desc.MipLevels = desc->MipLevels;
  staging_desc.MiscFlags = desc->MiscFlags;
  staging_desc.SampleDesc = sample_desc;
  // we want to read back immediately after copying, hence staging.
  staging_desc.Usage = D3D11_USAGE_STAGING;

  // multiply by 2 if desc->arraysize is 2...or multiply by arraysize. Wait, no,
  // ArraySize can be 0.
  if (desc->ArraySize == 2) {
    width_ = staging_desc.Width = desc->Width * 2;
  } else {
    width_ = staging_desc.Width = desc->Width;
  }

  height_ = desc->Height;
  texture_format_ = desc->Format;

  //in multipass case we already get a double-wide texture, so only multiply height by 2.
  //this will break single-pass but that's broken anyway rn.
  dst_y_ = static_cast<uint8_t*>(malloc(width_ * height_ * 2));
  dst_u_ = static_cast<uint8_t*>(malloc((width_ / 2) * (height_ /*/ 2*/)));
  dst_v_ = static_cast<uint8_t*>(malloc((width_ / 2) * (height_ /*/ 2*/)));

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

  //HACK: we just wanna see if color+depth combined works at all for us.
  staging_desc.Format = DXGI_FORMAT_R16_TYPELESS;
  hr = device_->CreateTexture2D(&staging_desc, nullptr, depth_staging_texture_.put());
   if (FAILED(hr)) {
    RTC_LOG_GLE_EX(LS_ERROR, hr) << "Failed creating the staging texture. The "
                                    "D3D11 frame source will not work.";
  }
  name = "D3D11VideoFrameSource_Depth_Staging";
  depth_staging_texture_->SetPrivateData(WKPDID_D3DDebugObjectName, name.length(),
                                   name.c_str());

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
  // int64_t time_us = rtc::TimeMicros();

  // int adapted_width;
  // int adapted_height;
  // int crop_width;
  // int crop_height;
  // int crop_x;
  // int crop_y;

  // // ok, so that's why we need the width of the whole frame, so webrtc knows
  // // about it. we can get that from staging_desc, though
  // if (!AdaptFrame(width_, height_, time_us, &adapted_width, &adapted_height,
  //                 &crop_width, &crop_height, &crop_x, &crop_y)) {
  //   return;
  // }

  // D3D11_TEXTURE2D_DESC desc = {};
  // rendered_image->GetDesc(&desc);

  // auto d3dFrameBuffer = D3D11VideoFrameBuffer::Create(
  //     context_.get(), staging_texture_.get(), rendered_image, dst_y_, dst_u_,
  //     dst_v_, desc);

  // // on windows, the best way to do this would be to convert to nv12 directly
  // // since the encoder expects that. libyuv features an ARGBToNV12 function. The
  // // problem now is, which frame type do we use? There's none for nv12. I guess
  // // we'd need to modify. Then we could also introduce d3d11 as frame type.
  // auto i420Buffer = d3dFrameBuffer->ToI420();

  // // TODO: set more stuff if needed, such as ntp timestamp
  // auto frame = VideoFrame::Builder()
  //                  .set_video_frame_buffer(i420Buffer)
  //                  .set_timestamp_us(time_us)
  //                  .build();
  // frame.set_xr_timestamp(timestamp);
  // OnFrame(frame);
}

void D3D11VideoFrameSource::OnFrameCaptured(ID3D11Texture2D* rendered_image,
                                            ID3D11Texture2D* depth_image,
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

  D3D11_TEXTURE2D_DESC desc = {};
  rendered_image->GetDesc(&desc);

  //TODO: pass depth into this and copy...wait, now we have 2 different tex formats, how would that work?
  // we cannot use copysubresourceregion here. Could we tell unity to use a certain depth format?
  //It'd still be incompatible with RGBA. Fuck. How do we do this? We need to pass a texture from the outside?
  //Fuck off. I don't feel like setting up shaders in lib code and all that...maybe use the engine somehow to set
  //a global or whatever, then use that. But wait, we still have R16 to R8...so we need to pack shit into RGBA now?
  //For alpha this is needed anyway, well...research time I guess? Could use that blog post from aras p.
  //Or we do the hacky thing and pack it the way we do now but our framebuffer is larger so we write at the offset we need...
  //How does it look at YUV level? We have a double-height double-width texture for Y and height, width for U and V planes.
  // +-----------------------+
  // |                       |
  // |                       |
  // |       Y color         |
  // |                       |
  // +-----------------------+
  // |                       |
  // |                       |
  // |       Y depth         |
  // |                       |
  // +-----------+-----------+
  // |           |           |
  // |           |           |
  // |  U color  |  V color  |
  // |           |           |
  // +-----------+-----------+
  // |           |           |
  // |           |           |
  // |  U depth  |  V depth  |
  // |           |           |
  // +-----------+-----------+

  //we still need a second staging texture for the depth, smh...otherwise map won't work. Fuck.
  //but if we sized our dst_y/u/v accordingly, could we reuse our current code?
  //2 staging textures are needed (different sizes?) for calling map.
  auto d3dFrameBuffer = D3D11VideoFrameBuffer::Create(
      context_.get(), staging_texture_.get(), rendered_image, depth_staging_texture_.get(), depth_image, dst_y_, dst_u_,
      dst_v_, desc);

  // on windows, the best way to do this would be to convert to nv12 directly
  // since the encoder expects that. libyuv features an ARGBToNV12 function. The
  // problem now is, which frame type do we use? There's none for nv12. I guess
  // we'd need to modify. Then we could also introduce d3d11 as frame type.
  auto i420Buffer = d3dFrameBuffer->ToI420();

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
