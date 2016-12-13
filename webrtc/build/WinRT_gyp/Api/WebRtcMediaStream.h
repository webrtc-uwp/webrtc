
// Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef WEBRTC_BUILD_WINRT_GYP_API_WEBRTCMEDIASTREAM_H_
#define WEBRTC_BUILD_WINRT_GYP_API_WEBRTCMEDIASTREAM_H_

#include <wrl.h>
#include <mfidl.h>
#include <vector>
#include "webrtc/api/mediastreaminterface.h"
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"
#include "webrtc/base/scoped_ptr.h"
#include "Media.h"
#include "MediaSourceHelper.h"

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::RuntimeClass;
using Microsoft::WRL::RuntimeClassFlags;
using Microsoft::WRL::RuntimeClassType;
using Windows::System::Threading::ThreadPoolTimer;

namespace Org {
	namespace WebRtc {
		namespace Internal {

			class WebRtcMediaSource;

			class WebRtcMediaStream :
				public RuntimeClass<RuntimeClassFlags<RuntimeClassType::WinRtClassicComMix>,
				IMFMediaStream, IMFMediaEventGenerator,
				IMFGetService>,
				public webrtc::VideoRendererInterface {
				InspectableClass(L"WebRtcMediaStream", BaseTrust)
			public:
				WebRtcMediaStream();
				virtual ~WebRtcMediaStream();
				HRESULT RuntimeClassInitialize(
					WebRtcMediaSource* source,
					Org::WebRtc::MediaVideoTrack^ track,
					String^ id);


				// IMFMediaEventGenerator
				IFACEMETHOD(GetEvent)(DWORD dwFlags, IMFMediaEvent **ppEvent);
				IFACEMETHOD(BeginGetEvent)(IMFAsyncCallback *pCallback, IUnknown *punkState);
				IFACEMETHOD(EndGetEvent)(IMFAsyncResult *pResult, IMFMediaEvent **ppEvent);
				IFACEMETHOD(QueueEvent)(MediaEventType met, const GUID& guidExtendedType,
					HRESULT hrStatus, const PROPVARIANT *pvValue);
				// IMFMediaStream
				IFACEMETHOD(GetMediaSource)(IMFMediaSource **ppMediaSource);
				IFACEMETHOD(GetStreamDescriptor)(IMFStreamDescriptor **ppStreamDescriptor);
				IFACEMETHOD(RequestSample)(IUnknown *pToken);
				// IMFGetService
				IFACEMETHOD(GetService)(REFGUID guidService, REFIID riid, LPVOID *ppvObject);

				// VideoRendererInterface
				virtual void RenderFrame(const cricket::VideoFrame *frame);

				STDMETHOD(Start)(IMFPresentationDescriptor *pPresentationDescriptor,
					const GUID *pguidTimeFormat, const PROPVARIANT *pvarStartPosition);
				STDMETHOD(Stop)();
				STDMETHOD(Shutdown)();
				STDMETHOD(SetD3DManager)(ComPtr<IMFDXGIDeviceManager> manager);

				rtc::scoped_ptr<webrtc::CriticalSectionWrapper> _lock;

			private:
				ComPtr<IMFMediaEventQueue> _eventQueue;

				WebRtcMediaSource* _source;
				Org::WebRtc::MediaVideoTrack^ _track;
				String^ _id;

				static HRESULT CreateMediaType(unsigned int width, unsigned int height,
					unsigned int rotation, IMFMediaType** ppType, bool isH264);
				HRESULT MakeSampleCallback(const cricket::VideoFrame* frame, IMFSample** sample);
				void FpsCallback(int fps);

				HRESULT ReplyToSampleRequest();

				rtc::scoped_ptr<MediaSourceHelper> _helper;

				ComPtr<IMFMediaType> _mediaType;
				ComPtr<IMFDXGIDeviceManager> _deviceManager;
				ComPtr<IMFStreamDescriptor> _streamDescriptor;
				ULONGLONG _startTickCount;
				ULONGLONG _frameCount;
				int _index;
				ULONG _frameReady;
				ULONG _frameBeingQueued;

				// From the sample manager.
				HRESULT ResetMediaBuffers();
				static const int BufferCount = 3;
				std::vector<ComPtr<IMFMediaBuffer>> _mediaBuffers;
				int _frameBufferIndex;

				bool _gpuVideoBuffer;
				bool _isH264;
				bool _started;
			};
		}
	}
}  // namespace Org.WebRtc.Internal

#endif  // WEBRTC_BUILD_WINRT_GYP_API_WEBRTCMEDIASTREAM_H_
