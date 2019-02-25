#ifndef HOLOLIGHT_D3D11VIDEOFRAMESOURCE
#define HOLOLIGHT_D3D11VIDEOFRAMESOURCE

#include "media/base/adaptedvideotracksource.h"
#include "rtc_base/asyncinvoker.h"

#include <winrt/base.h>
#include <d3d11.h>

namespace hololight {
    class D3D11VideoFrameSource : public rtc::AdaptedVideoTrackSource {
        //alright, so what does this need?
        //pretty much what androidvideotracksource does, for starters.
        //to test this we probably need to create the track from this source somehow.
        //basically we can copy the frame to a temp texture that we own and then call OnFrame.
        //then we need to find out what happens with the frames, encoding etc.

        //Threading in this lib is all over the place, and engines have their own threading considerations
        //so let's not forget this. We might need a thread checker like android or other impls.

        //also, we need our own videoframebuffer and videoframe implementation. again, look at android.
        public:
        static rtc::scoped_refptr<hololight::D3D11VideoFrameSource> 
            Create(ID3D11Device* device, ID3D11DeviceContext* context, D3D11_TEXTURE2D_DESC* desc, rtc::Thread* signaling_thread);
        

        //I guess somebody else calls this when a frame is available, then we call this->OnFrame which
        //notifies all attached sinks. That's what I get from android code in CameraXSession.java which
        //gets an event from android framework, creates a webrtc::VideoFrame and calls onFrameCaptured
        //which calls into native OnFrameCaptured.
        void OnFrameCaptured(ID3D11Texture2D* rendered_image);

        absl::optional<bool> needs_denoising() const override;

        bool is_screencast() const override;

        rtc::AdaptedVideoTrackSource::SourceState state() const override;

        bool remote() const override;

        void SetState(rtc::AdaptedVideoTrackSource::SourceState state);

        //TODO: we might need an overload with 2 textures because not everyone will use
        //texure arrays or double-wide.

        protected:
        D3D11VideoFrameSource(ID3D11Device* device, ID3D11DeviceContext* context, D3D11_TEXTURE2D_DESC* desc, rtc::Thread* signaling_thread);
        // D3D11VideoFrameSource(ID3D11Device* device, ID3D11DeviceContext* context, D3D11_TEXTURE2D_DESC* desc, rtc::Thread* signaling_thread, uint8_t* dst_y, uint8_t* dst_u, uint8_t* dst_v);

        private:
        winrt::com_ptr<ID3D11Texture2D> staging_texture_;
        winrt::com_ptr<ID3D11Device> device_;
        winrt::com_ptr<ID3D11DeviceContext> context_;
        int width_;
        int height_;

        uint8_t* dst_y_;
        uint8_t* dst_u_;
        uint8_t* dst_v_;

        rtc::Thread* signaling_thread_;
        rtc::AdaptedVideoTrackSource::SourceState state_ = rtc::AdaptedVideoTrackSource::SourceState::kInitializing;
        rtc::AsyncInvoker invoker_;
        const bool is_screencast_;
    };
}

#endif // HOLOLIGHT_D3D11VIDEOFRAMESOURCE