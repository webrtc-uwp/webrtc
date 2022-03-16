/*
*  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
*
*  Use of this source code is governed by a BSD-style license
*  that can be found in the LICENSE file in the root of the source
*  tree. An additional intellectual property rights grant can be found
*  in the file PATENTS.  All contributing project authors may
*  be found in the AUTHORS file in the root of the source tree.
*/

#ifndef HOLOLIGHT_D3D11VIDEOFRAMESOURCE
#define HOLOLIGHT_D3D11VIDEOFRAMESOURCE

#include "media/base/adaptedvideotracksource.h"
#include "rtc_base/asyncinvoker.h"
#include "common_types.h"

#include <winrt/base.h>
#include <d3d11.h>

namespace hlr {
    class D3D11VideoFrameSource : public rtc::AdaptedVideoTrackSource {
        //Threading in this lib is all over the place, and engines have their own threading considerations
        //so let's not forget this. We might need a thread checker like android or other impls.
        public:
        static rtc::scoped_refptr<D3D11VideoFrameSource>
            Create(ID3D11Device* device, ID3D11DeviceContext* context, D3D11_TEXTURE2D_DESC* color_desc, D3D11_TEXTURE2D_DESC* depth_desc, rtc::Thread* signaling_thread);

        ~D3D11VideoFrameSource() override;

        void OnFrameCaptured(ID3D11Texture2D* rendered_image, ID3D11Texture2D* depth_image, webrtc::XRTimestamp timestamp);

        absl::optional<bool> needs_denoising() const override;

        bool is_screencast() const override;

        rtc::AdaptedVideoTrackSource::SourceState state() const override;

        bool remote() const override;

        void SetState(rtc::AdaptedVideoTrackSource::SourceState state);

        protected:
        D3D11VideoFrameSource(ID3D11Device* device, ID3D11DeviceContext* context, D3D11_TEXTURE2D_DESC* color_desc, D3D11_TEXTURE2D_DESC* depth_desc, rtc::Thread* signaling_thread);

        private:
        winrt::com_ptr<ID3D11Texture2D> color_staging_texture_;
        winrt::com_ptr<ID3D11Texture2D> depth_staging_texture_;
        winrt::com_ptr<ID3D11Texture2D> depth_staging_texture_array_;
        winrt::com_ptr<ID3D11Device> device_;
        winrt::com_ptr<ID3D11DeviceContext> context_;
        int width_;
        int height_;

        size_t frame_mem_arena_size_ = 0;
        uint8_t* frame_mem_arena_ = nullptr;

        rtc::Thread* signaling_thread_;
        rtc::AdaptedVideoTrackSource::SourceState state_ = rtc::AdaptedVideoTrackSource::SourceState::kInitializing;
        rtc::AsyncInvoker invoker_;
        const bool is_screencast_;

        void SetupNV12Shaders(D3D11_TEXTURE2D_DESC* color_desc);
        void ConvertToNV12();
        winrt::com_ptr<ID3DBlob> CompileShader(const char* hlsl, const char* entrypoint, const char* shaderTarget);

        // Shaders for converting to NV12 in GPU
        winrt::com_ptr<ID3D11Texture2D>         textureNV12;
        winrt::com_ptr<ID3D11VertexShader>      vShaderToNV12_;
        winrt::com_ptr<ID3D11PixelShader>       pShaderToNV12_Y;
        winrt::com_ptr<ID3D11PixelShader>       pShaderToNV12_UV;
        winrt::com_ptr<ID3D11RenderTargetView>  renderTargetViewNV12Y;
        winrt::com_ptr<ID3D11RenderTargetView>  renderTargetViewNV12UV;
        winrt::com_ptr<ID3D11InputLayout> m_inputLayout;
		winrt::com_ptr<ID3D11Buffer> m_vertexBuffer;
		winrt::com_ptr<ID3D11Buffer> m_indexBuffer;
        winrt::com_ptr<ID3D11SamplerState> m_samplerState;
        winrt::com_ptr<ID3D11DepthStencilState> m_depthStencilState;
        winrt::com_ptr<ID3D11ShaderResourceView> color_texture_srv;
        D3D11_VIEWPORT nv12DrawingViewport;
        bool is_once_SRV = false;

        inline void SetD3DDebugName(ID3D11DeviceChild* obj, const std::string& name)
        {
            assert(obj);
            assert(!name.empty());
            obj->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<uint32_t>(name.size()), name.c_str());
        }
    };
}

#endif // HOLOLIGHT_D3D11VIDEOFRAMESOURCE
