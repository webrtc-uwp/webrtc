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

#include "third_party/winuwp_h264/native_handle_buffer.h"

// Media Foundation
#include <mfapi.h>
#include <mfplay.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <Codecapi.h>

// D3D
#include <D3Dcompiler.h>
#pragma comment(lib, "D3DCompiler.lib")
#include <thread>

using webrtc::VideoFrame;

namespace hlr {

// #if GpuVideoFrameBuffer
// // Used to store a IMFSample native handle buffer for a VideoFrame
// class MFNativeHandleBuffer : public webrtc::NativeHandleBuffer {
//  public:
//   static rtc::scoped_refptr<MFNativeHandleBuffer> Create(cricket::FourCC fourCC,
//                        const winrt::com_ptr<IMFSample>& sample,
//                        int width,
//                        int height){
//     return new rtc::RefCountedObject<MFNativeHandleBuffer>(fourCC, sample, width, height);
//   };

//   MFNativeHandleBuffer(cricket::FourCC fourCC,
//                        const winrt::com_ptr<IMFSample>& sample,
//                        int width,
//                        int height)
//       : NativeHandleBuffer(sample.get(), width, height), fourCC_(fourCC), sample_(sample)/*, i420Buffer_(i420Buffer)*/ {}

//   ~MFNativeHandleBuffer() override {}

//   rtc::scoped_refptr<webrtc::I420BufferInterface> ToI420() override { return nullptr; }

//   cricket::FourCC fourCC() const override { return fourCC_; }

//  private:
//   cricket::FourCC fourCC_{};
//   winrt::com_ptr<IMFSample> sample_;
// };
// #endif

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

  width_ = color_desc->Width * color_desc->ArraySize;
  height_ = color_desc->Height;

  if(color_desc->Format == DXGI_FORMAT_NV12){
    is_passthrough_ = true;
    return;
  }

#if !GpuVideoFrameBuffer
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

#else
  D3D11_TEXTURE2D_DESC flattened_desc = *color_desc;
  flattened_desc.Width = width_;
  flattened_desc.ArraySize = 1;
  flattened_desc.SampleDesc = DXGI_SAMPLE_DESC{ /*Count*/ 1, /*Quality*/ 0 };
  flattened_desc.Usage = D3D11_USAGE_DEFAULT;
  flattened_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

  HRESULT hr = S_OK;
  ON_SUCCEEDED(device_->CreateTexture2D(&flattened_desc, nullptr, flattened_color_texture_.put()));
  RTC_CHECK(SUCCEEDED(hr)) << "Failed creating the flattened colour texture. The D3D11 frame source will not work.";

  // If we get a desc for depth, allocate a staging texture for depth
  if (depth_desc != nullptr) {
    height_ *= 2;
    flattened_desc.Format = depth_desc->Format;

    ON_SUCCEEDED(device_->CreateTexture2D(&flattened_desc, nullptr, flattened_depth_texture_.put()));
    RTC_CHECK(SUCCEEDED(hr)) << "Failed creating the flattened depth texture. The D3D11 frame source will not work.";

    flattened_desc.Width = depth_desc->Width;
    flattened_desc.ArraySize = depth_desc->ArraySize;
    ON_SUCCEEDED(device_->CreateTexture2D(&flattened_desc, nullptr, depth_texture_array_.put()));
    RTC_CHECK(SUCCEEDED(hr)) << "Failed creating the depth texture array. The D3D11 frame source will not work.";
  }

  SetupNV12Shaders(color_desc, depth_desc);
#endif
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
#if !GpuVideoFrameBuffer
  free(frame_mem_arena_);
#endif
}

void D3D11VideoFrameSource::OnFrameCaptured(ID3D11Texture2D* color_texture,
                                            ID3D11Texture2D* depth_texture,
                                            webrtc::XRTimestamp timestamp) {
  HRESULT hr = S_OK;
  int64_t time_us = rtc::TimeMicros();
  int adapted_width;
  int adapted_height;
  int crop_width;
  int crop_height;
  int crop_x;
  int crop_y;
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> frame_buffer;

  if (!AdaptFrame(width_, height_, time_us, &adapted_width, &adapted_height,
                  &crop_width, &crop_height, &crop_x, &crop_y)) {
    return;
  }

  #if !GpuVideoFrameBuffer
  auto d3d_frame_buffer = D3D11VideoFrameBuffer::Create(
      context_.get(),
      color_texture, color_staging_texture_.get(),
      depth_texture, depth_staging_texture_.get(), depth_staging_texture_array_.get()//,
      /* TODO: width_, height_ */ /* adapted_width, adapted_height */);

  // on windows, the best way to do this would be to convert to nv12 directly
  // since the encoder expects that. libyuv features an ARGBToNV12 function. The
  // problem now is, which frame type do we use? There's none for nv12. I guess
  // we'd need to modify. Then we could also introduce d3d11 as frame type.
  frame_buffer = d3d_frame_buffer->ToI420AlphaDepth(frame_mem_arena_, frame_mem_arena_size_);
  #else
  unsigned int lock_key = 0;
  unsigned int release_key = 1;
  winrt::com_ptr<ID3D11Texture2D> shared_nv12_texture;
  D3D11_TEXTURE2D_DESC color_desc, depth_desc;
  color_texture->GetDesc(&color_desc);
  if(depth_texture) depth_texture->GetDesc(&depth_desc);

  if(is_passthrough_){
    shared_nv12_texture.copy_from(color_texture);
  }
  else{
    if(color_desc.ArraySize == 1){
      context_->CopyResource(flattened_color_texture_.get(), color_texture);
      if(depth_texture){
        context_->CopyResource(flattened_depth_texture_.get(), depth_texture);
      }
    }
    else if (color_desc.ArraySize == 2)
    {
      // texture array (2 images)
      UINT left_eye_color_subresource = D3D11CalcSubresource(0, 0, color_desc.MipLevels);
      UINT right_eye_color_subresource = D3D11CalcSubresource(0, 1, color_desc.MipLevels);

      context_->CopySubresourceRegion(flattened_color_texture_.get(), 0, 0, 0, 0, color_texture, left_eye_color_subresource, 0);
      context_->CopySubresourceRegion(flattened_color_texture_.get(), 0, color_desc.Width, 0, 0, color_texture, right_eye_color_subresource, 0);
      if(depth_texture){
        // texture array (2 images)
        UINT left_eye_depth_subresource = D3D11CalcSubresource(0, 0, depth_desc.MipLevels);
        UINT right_eye_depth_subresource = D3D11CalcSubresource(0, 1, depth_desc.MipLevels);

        context_->CopyResource(depth_texture_array_.get(), depth_texture);
        context_->CopySubresourceRegion(flattened_depth_texture_.get(), 0, 0, 0, 0, depth_texture_array_.get(), left_eye_depth_subresource, 0);
        context_->CopySubresourceRegion(flattened_depth_texture_.get(), 0, depth_desc.Width, 0, 0, depth_texture_array_.get(), right_eye_depth_subresource, 0);
      }
    }
    else{
      RTC_LOG(LS_ERROR) << "Received a colour texture with ArraySize > 2";
    }

    if (!is_once_srv_)
    {
      auto const color_texture_resource_view = CD3D11_SHADER_RESOURCE_VIEW_DESC(flattened_color_texture_.get(),
                                                                            D3D11_SRV_DIMENSION_TEXTURE2D,
                                                                            DXGI_FORMAT_R8G8B8A8_UNORM);

      hr = device_->CreateShaderResourceView(flattened_color_texture_.get(),
                                        &color_texture_resource_view,
                                        color_texture_srv_.put());
      if(depth_texture){
        auto const depth_texture_resource_view = CD3D11_SHADER_RESOURCE_VIEW_DESC(flattened_depth_texture_.get(),
                                                                            D3D11_SRV_DIMENSION_TEXTURE2D,
                                                                            DXGI_FORMAT_R16_UNORM);

        hr = device_->CreateShaderResourceView(flattened_depth_texture_.get(),
                                        &depth_texture_resource_view,
                                        depth_texture_srv_.put());
      }

      SUCCEEDED(hr);
      is_once_srv_ = true;
    }

    // Convert from argb to nv12
    ConvertToNV12(color_texture, depth_texture);

    //Create copy nv12 texture description for passing to encoder
    D3D11_TEXTURE2D_DESC copyDesc = {};
    ZeroMemory(&copyDesc, sizeof(D3D11_TEXTURE2D_DESC));
    copyDesc.Width = width_;
    copyDesc.Height = height_;
    copyDesc.MipLevels = 1;
    copyDesc.ArraySize = 1;
    copyDesc.Format = DXGI_FORMAT_NV12;
    copyDesc.SampleDesc.Count = 1;
    copyDesc.SampleDesc.Quality = 0;
    copyDesc.Usage = D3D11_USAGE_DEFAULT;
    copyDesc.BindFlags = 0;
    // Since the encoder uses a different d3d device, the below flags are required to ensure synchronisation
    // with the rendering device
    copyDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

    // TODO: Potential overkill recreating the texture every frame. This requires further investigation.
    // Tracked in https://holo.atlassian.net/browse/IAR-509
    hr = device_->CreateTexture2D(&copyDesc, nullptr, shared_nv12_texture.put());

    //Make a copy our NV12 texture2D
    winrt::com_ptr<IDXGIKeyedMutex> keyed_mutex;
    hr = shared_nv12_texture->QueryInterface(keyed_mutex.put());
    RTC_CHECK(SUCCEEDED(hr)) << "Texture is not correctly shareable";

    hr = keyed_mutex->AcquireSync(lock_key, 5);
    if(FAILED(hr)){
      RTC_LOG(LS_WARNING) << "Failed to acquire the shared mutex in at converter within 5ms, dropping frame";
      return;
    }

    context_->CopySubresourceRegion(shared_nv12_texture.get(), 0, 0, 0, 0, render_target_nv12_color_.get(), 0, NULL);
    if(depth_texture){
      context_->CopySubresourceRegion(shared_nv12_texture.get(), 0, 0, color_desc.Height, 0, render_target_nv12_depth_.get(), 0, NULL);
    }
    keyed_mutex->ReleaseSync(release_key);
  }

  // Create buffer
	winrt::com_ptr<IMFMediaBuffer> nv12_buffer;
	hr = MFCreateDXGISurfaceBuffer(__uuidof(ID3D11Texture2D), shared_nv12_texture.get(), 0, FALSE, nv12_buffer.put());
  // (width * height) + (width / 2 * height / 2) + (width / 2 * height / 2) for YUV channels
  uint64_t buffer_length = (adapted_width * adapted_height) + (adapted_width * (adapted_height / 2));
  nv12_buffer->SetCurrentLength(buffer_length);
	// CHECK_HR(hr, "Failed to create IMFMediaBuffer");

	// Create sample
	winrt::com_ptr<IMFSample> nv12_sample;
	hr = MFCreateSample(nv12_sample.put());
	// CHECK_HR(hr, "Failed to create IMFSample");
	hr = nv12_sample->AddBuffer(nv12_buffer.get());
	// CHECK_HR(hr, "Failed to add buffer to IMFSample");

  // Pass in the lock/release keys flipped to keep synchronisation
  frame_buffer = new rtc::RefCountedObject<webrtc::MFNativeHandleBuffer>(
    cricket::FOURCC_NV12,
    nv12_sample,
    width_,
    height_,
    release_key,
    lock_key);
  #endif
  auto frame = VideoFrame::Builder()
                   .set_video_frame_buffer(frame_buffer)
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

#if GpuVideoFrameBuffer
void D3D11VideoFrameSource::SetupNV12Shaders(D3D11_TEXTURE2D_DESC* color_desc, D3D11_TEXTURE2D_DESC* depth_desc)
{
  HRESULT hr = S_OK;
  const char* vShader;
  const char* pShaderUV;
  const char* pShaderY;

  if(!depth_desc){
    vShader = R"_(

    struct VertexInputType
    {
      float2 uv: TEXCOORD0;
      uint id: SV_InstanceID;
    };

    struct PixelInputType
    {
        float4 position : SV_POSITION;
        float2 tex : TEXCOORD0;
    };

    PixelInputType main(VertexInputType input)
    {
      PixelInputType output;

      output.tex = input.uv;
      output.position = float4((output.tex.x - 0.5f) * 2, -(output.tex.y-0.5f) * 2, 0, 1);

      return output;
    })_";

    pShaderY = R"_(

    struct PixelInputType
    {
        float4 position : SV_POSITION;
        float2 tex : TEXCOORD0;
    };

    Texture2D<float4> color_texture : register(t0);

    SamplerState samp_color : register(s0);

    float main(PixelInputType input) : SV_TARGET
    {
      float4 sampledColor =  color_texture.Sample(samp_color, input.tex);

      // Range 0-1
      float ColorY = (0.257f * sampledColor.b + 0.504f * sampledColor.g + 0.098f * sampledColor.r) + (16 / 256.0f);
      ColorY = clamp(ColorY, 0.0f, 1.0f);

      return ColorY;
    })_";

    pShaderUV = R"_(

    struct PixelInputType
    {
        float4 position : SV_POSITION;
        float2 tex : TEXCOORD0;
    };

    Texture2D<float4> color_texture : register(t0);

    SamplerState samp_color : register(s0);

    float2 main(PixelInputType input) : SV_TARGET
    {
      float4 sampledColor =  color_texture.Sample(samp_color, input.tex);

      // Range 0-1
      float ColorU = (-0.148f * sampledColor.b - 0.291f * sampledColor.g + 0.439f * sampledColor.r) + (128.0f / 256.0f);
      float ColorV = (0.439f * sampledColor.b - 0.368f * sampledColor.g - 0.071f * sampledColor.r) + (128.0f / 256.0f);

      ColorU = clamp(ColorU, 0.0f, 1.0f);
      ColorV = clamp(ColorV, 0.0f, 1.0f);

      return float2(ColorU, ColorV);
      //return input.tex;
    })_";
  }
  else{
    vShader = R"_(

    struct VertexInputType
    {
      float2 uv: TEXCOORD0;
      uint id: SV_InstanceID;
    };

    struct PixelInputType
    {
        float4 position : SV_POSITION;
        float2 tex : TEXCOORD0;
    };

    PixelInputType main(VertexInputType input)
    {
      PixelInputType output;

      output.tex = input.uv;
      output.position = float4((output.tex.x - 0.5f) * 2, -(output.tex.y-0.5f) * 2, 0, 1);

      return output;
    })_";

    pShaderY = R"_(

    struct PixelInputType
    {
        float4 position : SV_POSITION;
        float2 tex : TEXCOORD0;
    };

    struct YOutput
    {
      float color : SV_Target0;
      float depth : SV_Target1;
    };

    Texture2D<float4> color_texture : register(t0);
    Texture2D<float> depth_texture : register(t1);

    SamplerState texture_sampler : register(s0);

    YOutput main(PixelInputType input)
    {
      YOutput output;
      float4 sampledColor = color_texture.Sample(texture_sampler, input.tex);

      // Range 0-1
      output.color = (0.257f * sampledColor.b + 0.504f * sampledColor.g + 0.098f * sampledColor.r) + (16 / 256.0f);

      float depth = depth_texture.Sample(texture_sampler, input.tex);
      uint highDepth = depth * 65535.0f;
      output.depth = (highDepth >> 8) / 255.0f;

      return output;
    })_";

    pShaderUV = R"_(

    struct PixelInputType
    {
        float4 position : SV_POSITION;
        float2 tex : TEXCOORD0;
    };

    struct UVOutput
    {
      float2 color : SV_Target0;
      float2 depth_alpha : SV_Target1;
    };

    Texture2D<float4> color_texture : register(t0);
    Texture2D<float> depth_texture : register(t1);

    SamplerState texture_sampler : register(s0);

    UVOutput main(PixelInputType input)
    {
      UVOutput output;
      float4 sampledColor =  color_texture.Sample(texture_sampler, input.tex);

      // Range 0-1
      float ColorU = (-0.148f * sampledColor.b - 0.291f * sampledColor.g + 0.439f * sampledColor.r) + (128.0f / 256.0f);
      float ColorV = (0.439f * sampledColor.b - 0.368f * sampledColor.g - 0.071f * sampledColor.r) + (128.0f / 256.0f);

      output.color = float2(ColorU, ColorV);

      float depth = depth_texture.Sample(texture_sampler, input.tex);
      uint lowDepth = depth * 65535.0f;
      lowDepth = lowDepth & 0xFF;

      output.depth_alpha = float2(lowDepth / 255.0f, sampledColor.a);

      return output;
    })_";
  }

  //Create & compile shaders
  const winrt::com_ptr<ID3DBlob> vertexShaderBytes = CompileShader(vShader, "main", "vs_5_0");
  const winrt::com_ptr<ID3DBlob> pShaderBytesY = CompileShader(pShaderY, "main", "ps_5_0");
  const winrt::com_ptr<ID3DBlob> pShaderBytesUV = CompileShader(pShaderUV, "main", "ps_5_0");

  hr = device_->CreateVertexShader(vertexShaderBytes->GetBufferPointer(),
                              vertexShaderBytes->GetBufferSize(),
                              nullptr,
                              v_shader_to_nv12_.put());

  hr = device_->CreatePixelShader(pShaderBytesY->GetBufferPointer(),
                              pShaderBytesY->GetBufferSize(),
                              nullptr,
                              p_shader_to_nv12_y_.put());

  hr = device_->CreatePixelShader(pShaderBytesUV->GetBufferPointer(),
                          pShaderBytesUV->GetBufferSize(),
                          nullptr,
                          p_shader_to_nv12_uv_.put());

  //Setup vertex buffers
  constexpr std::array<DirectX::XMFLOAT2, 4> billboardVertices
  { {
      DirectX::XMFLOAT2(0, 0),
      DirectX::XMFLOAT2(0, 1),
      DirectX::XMFLOAT2(1, 1),
      DirectX::XMFLOAT2(1, 0)
  } };

  constexpr std::array<unsigned short, 6> billboardIndices =
  { {
      0,1,2,
      0,2,3
  } };


  constexpr std::array<D3D11_INPUT_ELEMENT_DESC, 1> vertexDesc =
  { {
    {   "TEXCOORD",
        0,
        DXGI_FORMAT_R32G32_FLOAT,
        0,
        0,
        D3D11_INPUT_PER_VERTEX_DATA,
        0
    }
  } };

  //Create input layout
  hr = device_->CreateInputLayout(
    vertexDesc.data(), static_cast<UINT>(vertexDesc.size()),
    vertexShaderBytes->GetBufferPointer(), static_cast<UINT>(vertexShaderBytes->GetBufferSize()),
    input_layout_.put()
  );

  D3D11_SUBRESOURCE_DATA vertexBufferData;
  vertexBufferData.pSysMem = billboardVertices.data();
  vertexBufferData.SysMemPitch = 0;
  vertexBufferData.SysMemSlicePitch = 0;
  const CD3D11_BUFFER_DESC vertexBufferDesc(sizeof(DirectX::XMFLOAT2) * static_cast<UINT>(billboardVertices.size()), D3D11_BIND_VERTEX_BUFFER);
  hr = device_->CreateBuffer(&vertexBufferDesc, &vertexBufferData, vertex_buffer_.put());

  D3D11_SUBRESOURCE_DATA indexBufferData;
  indexBufferData.pSysMem = billboardIndices.data();
  indexBufferData.SysMemPitch = 0;
  indexBufferData.SysMemSlicePitch = 0;
  const CD3D11_BUFFER_DESC indexBufferDesc(sizeof(unsigned short) * static_cast<UINT>(billboardIndices.size()), D3D11_BIND_INDEX_BUFFER);
  hr = device_->CreateBuffer(&indexBufferDesc, &indexBufferData, index_buffer_.put());

  //Create nv12 texture description
  D3D11_TEXTURE2D_DESC nv12TextDesc = {};
  ZeroMemory(&nv12TextDesc, sizeof(D3D11_TEXTURE2D_DESC));
  nv12TextDesc.Width = width_;
  nv12TextDesc.Height = color_desc->Height; // Don't use height_ incase we have depth
  nv12TextDesc.MipLevels = 1;
  nv12TextDesc.ArraySize = 1;
  nv12TextDesc.Format = DXGI_FORMAT_NV12;
  nv12TextDesc.SampleDesc.Count = 1;
  nv12TextDesc.SampleDesc.Quality = 0;
  nv12TextDesc.Usage = D3D11_USAGE_DEFAULT;
  nv12TextDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
  nv12TextDesc.CPUAccessFlags = 0;
  nv12TextDesc.MiscFlags = 0;

  // Sampler for rgba textures
  D3D11_SAMPLER_DESC sampler_desc;
  ZeroMemory(&sampler_desc, sizeof(D3D11_SAMPLER_DESC));
  sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
  sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
  sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
  sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
  sampler_desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
  sampler_desc.BorderColor[0] = 0.972f;
  sampler_desc.BorderColor[1] = 0.180f;
  sampler_desc.BorderColor[2] = 1.0f;
  sampler_desc.BorderColor[3] = 1.0f;

  //Create our sampler
  hr = device_->CreateSamplerState(&sampler_desc, sampler_state_.put());

  //Create our NV12 texture2D
  hr = device_->CreateTexture2D(&nv12TextDesc, nullptr, render_target_nv12_color_.put());
  SetD3DDebugName(render_target_nv12_color_.get(), "RNV12TextureColor");
  SUCCEEDED(hr);

  if(depth_desc){
    hr = device_->CreateTexture2D(&nv12TextDesc, nullptr, render_target_nv12_depth_.put());
    SetD3DDebugName(render_target_nv12_depth_.get(), "RNV12TextureDepth");
    SUCCEEDED(hr);
  }

  //Set viewport
  nv12_drawing_viewport_ = {};
  nv12_drawing_viewport_.Width  = color_desc->Width;
  nv12_drawing_viewport_.Height = color_desc->Height;
  nv12_drawing_viewport_.MaxDepth = D3D11_MAX_DEPTH;
  nv12_drawing_viewport_.MinDepth = 0;

  //Set stencil & depth state
  D3D11_DEPTH_STENCIL_DESC dsDesc;
  ZeroMemory(&dsDesc, sizeof(D3D11_DEPTH_STENCIL_DESC));
  dsDesc.DepthEnable = false;
  dsDesc.StencilEnable = false;
  hr = device_->CreateDepthStencilState(&dsDesc, depth_stencil_state_.put());

  //Set our render targets
  CD3D11_RENDER_TARGET_VIEW_DESC nv12RTVDescYColor;
  ZeroMemory(&nv12RTVDescYColor, sizeof(CD3D11_RENDER_TARGET_VIEW_DESC));
  nv12RTVDescYColor.Format = DXGI_FORMAT_R8_UNORM;
  nv12RTVDescYColor.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

  CD3D11_RENDER_TARGET_VIEW_DESC nv12RTVDescUVColor;
  ZeroMemory(&nv12RTVDescUVColor, sizeof(CD3D11_RENDER_TARGET_VIEW_DESC));
  nv12RTVDescUVColor.Format = DXGI_FORMAT_R8G8_UNORM;
  nv12RTVDescUVColor.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

  hr = device_->CreateRenderTargetView(render_target_nv12_color_.get(),
                                  &nv12RTVDescYColor,
                                  render_target_view_nv12_y_color_.put());

  hr = device_->CreateRenderTargetView(render_target_nv12_color_.get(),
                                  &nv12RTVDescUVColor,
                                  render_target_view_nv12_uv_color_.put());

  if(depth_desc){
    hr = device_->CreateRenderTargetView(render_target_nv12_depth_.get(),
                                    &nv12RTVDescYColor,
                                    render_target_view_nv12_y_depth_.put());

    hr = device_->CreateRenderTargetView(render_target_nv12_depth_.get(),
                                    &nv12RTVDescUVColor,
                                    render_target_view_nv12_uv_depth_.put());
  }

  SUCCEEDED(hr);
}

void D3D11VideoFrameSource::ConvertToNV12(ID3D11Texture2D* color_texture, ID3D11Texture2D* depth_texture)
{
  D3D11_TEXTURE2D_DESC color_desc;
  color_texture->GetDesc(&color_desc);
  //Temp wrappers & others for render passes
  float clearcolor [] = {0,0,0,0};
  std::vector<ID3D11RenderTargetView*> yRenderTargets;
  std::vector<ID3D11RenderTargetView*> uvRenderTargets;
  std::vector<ID3D11ShaderResourceView*> shaderResourceTextures;

  yRenderTargets.push_back(render_target_view_nv12_y_color_.get());
  uvRenderTargets.push_back(render_target_view_nv12_uv_color_.get());
  shaderResourceTextures.push_back(color_texture_srv_.get());

  if(depth_texture){
    yRenderTargets.push_back(render_target_view_nv12_y_depth_.get());
    uvRenderTargets.push_back(render_target_view_nv12_uv_depth_.get());
    shaderResourceTextures.push_back(depth_texture_srv_.get());
  }

  ID3D11SamplerState * samplers[] = { sampler_state_.get() };

  //Set vertex shader
  context_->VSSetShader(v_shader_to_nv12_.get(), nullptr, 0);

  // Each vertex is one instance of the XMFLOAT2 struct.
  const UINT stride = sizeof(DirectX::XMFLOAT2);
  const UINT offset = 0;
  ID3D11Buffer* pBuffer = vertex_buffer_.get();
  context_->IASetVertexBuffers(
    0,
    1,
    &pBuffer,
    &stride,
    &offset
  );

  context_->IASetIndexBuffer(
    index_buffer_.get(),
    DXGI_FORMAT_R16_UINT, // Each index is one 16-bit unsigned integer (short).
    0
  );

  // Set primitive layout & stuff
  context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  context_->IASetInputLayout(input_layout_.get());
  context_->RSSetViewports(1, &nv12_drawing_viewport_);
  context_->OMSetBlendState(NULL, nullptr, 0xffffffff);
  context_->OMSetDepthStencilState(depth_stencil_state_.get(), 1);

  //----------------------First Render Pass----------------------------------------------

  //Set viewport to original again
  nv12_drawing_viewport_.Width  = width_;
  nv12_drawing_viewport_.Height = color_desc.Height;

  context_->OMSetRenderTargets(yRenderTargets.size(), yRenderTargets.data(), nullptr);
  context_->RSSetViewports(1, &nv12_drawing_viewport_);
  for(auto* target : yRenderTargets){
    context_->ClearRenderTargetView(target, clearcolor);
  }
  context_->PSSetShader(p_shader_to_nv12_y_.get(), nullptr, 0);
  context_->PSSetShaderResources(0, shaderResourceTextures.size(), shaderResourceTextures.data());
  context_->PSSetSamplers(0, 1, samplers);
  context_->DrawIndexed(6, 0, 0);

  //----------------------Second Render Pass----------------------------------------------
  nv12_drawing_viewport_.Width  /= 2;
  nv12_drawing_viewport_.Height /= 2;

  context_->OMSetRenderTargets(uvRenderTargets.size(), uvRenderTargets.data(), nullptr);
  context_->RSSetViewports(1, &nv12_drawing_viewport_);
  for(auto* target : uvRenderTargets){
    context_->ClearRenderTargetView(target, clearcolor);
  }
  context_->PSSetShader(p_shader_to_nv12_uv_.get(), nullptr, 0);
  context_->PSSetShaderResources(0, shaderResourceTextures.size(), shaderResourceTextures.data());
  context_->PSSetSamplers(0, 1, samplers);
  context_->DrawIndexed(6, 0, 0);
}

winrt::com_ptr<ID3DBlob> D3D11VideoFrameSource::CompileShader(const char* hlsl, const char* entrypoint, const char* shaderTarget) {
  winrt::com_ptr<ID3DBlob> compiled;
  winrt::com_ptr<ID3DBlob> errMsgs;
  DWORD flags = D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR | D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS;

#ifndef NDEBUG
  flags |= D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_DEBUG;
#else
  flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

  HRESULT hr =
      D3DCompile(hlsl, strlen(hlsl), nullptr, nullptr, nullptr, entrypoint, shaderTarget, flags, 0, compiled.put(), errMsgs.put());
  if (FAILED(hr)) {
      std::string errMsg((const char*)errMsgs->GetBufferPointer(), errMsgs->GetBufferSize());
      // DEBUG_PRINT("D3DCompile failed %X: %s", hr, errMsg.c_str());
      // CHECK_HRESULT(hr, "D3DCompile failed");
      RTC_LOG(LS_WARNING) << errMsg;
  }
  return compiled;
}
#endif
}  // namespace hlr
