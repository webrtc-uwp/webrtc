/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef HOLOLIGHT_D3D11VIDEOFRAMEBUFFER
#define HOLOLIGHT_D3D11VIDEOFRAMEBUFFER

#include <d3d11.h>
#include <winrt/base.h>

#include "api/video/video_frame_buffer.h"
#include "rtc_base/refcountedobject.h"

namespace hlr {

inline int div_ceiled_fast(int dividend, int divisor) { assert(dividend != 0); return 1 + ((dividend - 1) / divisor); /*if dividend != 0*/ };

// KL, doing MFT D3D11 work:
// I guess this should support the case where we don't care about i420 (even if
// webrtc says it's mandatory, or we just make a shitty but non-crashing version
// where we allocate buffers ourselves). That means we don't need the context
// and staging_texture, just rendered_image. Calling ToI420 will either allocate
// enough memory to hold the image or crash if context/staging_texture are
// missing. This case assumes we just want the texture to stay on the GPU and
// use it as a resource for rendering.

class D3D11VideoFrameBuffer : public webrtc::VideoFrameBuffer {
 protected:
  D3D11VideoFrameBuffer(ID3D11DeviceContext* context,
                        ID3D11Texture2D* color_texture, ID3D11Texture2D* color_staging_texture,
                        UINT subresource_index, UINT subresource_width, UINT subresource_height);

  D3D11VideoFrameBuffer(ID3D11DeviceContext* context,
                        ID3D11Texture2D* color_texture, ID3D11Texture2D* color_staging_texture,
                        ID3D11Texture2D* depth_texture, ID3D11Texture2D* depth_staging_texture, ID3D11Texture2D* depth_staging_texture_array);

 public:
  ~D3D11VideoFrameBuffer() override;

  // Used on client side. ToI420 would probably crash because it has no
  // buffers to copy to, since those are supplied from the outside. On the
  // client CPU download isn't needed as the texture is decoded on the GPU and stays there.
  // The only user of this (decoder) doesn't supply neither context nor staging_texture. Can we delete those?
  static rtc::scoped_refptr<D3D11VideoFrameBuffer> Create(
      ID3D11DeviceContext* context,
      ID3D11Texture2D* color_texture, ID3D11Texture2D* color_staging_texture,
      UINT subresource_index, UINT subreouce_width, UINT subresource_height);

  // Used on server side. Supports calling ToI420 (i.e. downloading to CPU)
  // because the encoder expects the data in this format.
  static rtc::scoped_refptr<D3D11VideoFrameBuffer> Create(
      ID3D11DeviceContext* context,
      ID3D11Texture2D* color_texture, ID3D11Texture2D* color_staging_texture,
      ID3D11Texture2D* depth_texture, ID3D11Texture2D* depth_staging_texture, ID3D11Texture2D* depth_staging_texture_array);

  webrtc::VideoFrameBuffer::Type type() const override;

  int width() const override;

  int height() const override;

  // Returns the subresource index for color_texture_.
  // This is needed because the decoder MFT allocates a texture array, so the
  // resource is always the same but we get different subresources each frame.
  uint32_t subresource_index() { return subresource_index_; }

  ID3D11Texture2D* texture() { return color_texture_.get(); }

  rtc::scoped_refptr<webrtc::I420BufferInterface> ToI420() override;
  rtc::scoped_refptr<webrtc::I420BufferInterface> ToI420(uint8_t* buffer, int capacity);
  uint8_t* ToNV12(uint8_t* IN OUT buffer, int capacity);
  // uint8_t* LoadToMemory(uint8_t* OUT destination, size_t capacity);
  rtc::scoped_refptr<webrtc::I420BufferInterface> ToI420_New(uint8_t* buffer, int capacity);
  uint8_t* ToNV12_New(uint8_t* IN OUT buffer, int capacity);

  rtc::scoped_refptr<webrtc::I420BufferInterface> ToI420AlphaDepth(uint8_t* buffer, int capacity);
  uint8_t* ToNV12AlphaDepth(uint8_t* IN OUT buffer, int capacity);

 protected:
  void StageTextures(); // TODO: pass staging texture as arg and make this public?
  template<typename T> HRESULT UseStagedResource(ID3D11Resource *resource, T action);
#if false
  bool DownloadColor();
  void DownloadDepth();
#endif

 private:
  winrt::com_ptr<ID3D11DeviceContext> context_;
  winrt::com_ptr<ID3D11Texture2D> color_texture_;
  D3D11_TEXTURE2D_DESC color_texture_desc_;
  UINT subresource_index_;
  // NOTE: not necessarily the size of the source texture dimensions (they could contain padding because the decoder outputs pot/even sizes)
  UINT subresource_width_;
  UINT subresource_height_;
  UINT width_; // total buffer width (color + depth)
  UINT height_; // total buffer height (color + depth)
  // DXGI_FORMAT color_texture_format_; // TODO: already contains width & height, do we need the redundancy?

  winrt::com_ptr<ID3D11Texture2D> depth_texture_;
  D3D11_TEXTURE2D_DESC depth_texture_desc_;
  // std::optional<DXGI_FORMAT> depth_texture_format_;

  // This is only used for loading data from the GPU.
  winrt::com_ptr<ID3D11Texture2D> color_staging_texture_;
  winrt::com_ptr<ID3D11Texture2D> depth_staging_texture_;
  // This one is used as a workaround for CopySubresourceRegion not working with BIND_DEPTH_STENCIL textures.
  winrt::com_ptr<ID3D11Texture2D> depth_staging_texture_array_;
};

}  // namespace hlr

#endif  // HOLOLIGHT_D3D11VIDEOFRAMEBUFFER
