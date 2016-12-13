
// Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef WEBRTC_BUILD_WINRT_GYP_API_WEBRTCMEDIASOURCE_H_
#define WEBRTC_BUILD_WINRT_GYP_API_WEBRTCMEDIASOURCE_H_

#include <wrl.h>
#include <WebRtcMediaStream.h>
#include <mfapi.h>
#include <mfidl.h>
#include <windows.media.h>
#include <windows.media.core.h>
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"
#include "webrtc/base/scoped_ptr.h"
#include "Media.h"

interface IFrameSource;

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::RuntimeClass;
using Microsoft::WRL::RuntimeClassFlags;
using Microsoft::WRL::RuntimeClassType;

namespace Org {
	namespace WebRtc {
		namespace Internal {

			class WebRtcMediaSource :
				public RuntimeClass<RuntimeClassFlags<RuntimeClassType::WinRtClassicComMix>,
				IMFMediaSourceEx, IMFMediaSource, IMFMediaEventGenerator,
				IMFGetService,
				IMFRateControl,
				IMFRateSupport,
				ABI::Windows::Media::Core::IMediaSource, IInspectable> {
				InspectableClass(L"WebRtcMediaSource", BaseTrust)
			public:
				static HRESULT CreateMediaSource(
					ABI::Windows::Media::Core::IMediaSource** source,
					Org::WebRtc::MediaVideoTrack^ track, String^ id);

				WebRtcMediaSource();
				virtual ~WebRtcMediaSource();
				HRESULT RuntimeClassInitialize(Org::WebRtc::MediaVideoTrack^ track, String^ id);

				// IMFMediaEventGenerator
				IFACEMETHOD(GetEvent)(DWORD dwFlags, IMFMediaEvent **ppEvent);
				IFACEMETHOD(BeginGetEvent)(IMFAsyncCallback *pCallback, IUnknown *punkState);
				IFACEMETHOD(EndGetEvent)(IMFAsyncResult *pResult, IMFMediaEvent **ppEvent);
				IFACEMETHOD(QueueEvent)(MediaEventType met, const GUID& guidExtendedType,
					HRESULT hrStatus, const PROPVARIANT *pvValue);
				// IMFMediaSource
				IFACEMETHOD(GetCharacteristics)(DWORD *pdwCharacteristics);
				IFACEMETHOD(CreatePresentationDescriptor)(
					IMFPresentationDescriptor **ppPresentationDescriptor);
				IFACEMETHOD(Start)(IMFPresentationDescriptor *pPresentationDescriptor,
					const GUID *pguidTimeFormat, const PROPVARIANT *pvarStartPosition);
				IFACEMETHOD(Stop)();
				IFACEMETHOD(Pause)();
				IFACEMETHOD(Shutdown)();
				// IMFMediaSourceEx
				IFACEMETHOD(GetSourceAttributes)(IMFAttributes **ppAttributes);
				IFACEMETHOD(GetStreamAttributes)(DWORD dwStreamIdentifier,
					IMFAttributes **ppAttributes);
				IFACEMETHOD(SetD3DManager)(IUnknown *pManager);
				// IMFGetService
				IFACEMETHOD(GetService)(REFGUID guidService, REFIID riid, LPVOID *ppvObject);
				// IMFRateControl
				IFACEMETHOD(SetRate)(BOOL fThin, float flRate);
				IFACEMETHOD(GetRate)(BOOL *pfThin, float *pflRate);
				// IMFRateSupport
				IFACEMETHOD(GetSlowestRate)(MFRATE_DIRECTION eDirection, BOOL fThin, float *pflRate);
				IFACEMETHOD(GetFastestRate)(MFRATE_DIRECTION eDirection, BOOL fThin, float *pflRate);
				IFACEMETHOD(IsRateSupported)(BOOL fThin, float flRate, float *pflNearestSupportedRate);

			private:
				rtc::scoped_ptr<webrtc::CriticalSectionWrapper> _lock;
				ComPtr<IMFMediaEventQueue> _eventQueue;
				ComPtr<WebRtcMediaStream> _stream;
				ComPtr<IMFPresentationDescriptor> _presDescriptor;
				ComPtr<IMFDXGIDeviceManager> _deviceManager;
				BOOL _rateControlThin;
				float _rate;
				bool _started;
			};
		}
	}
}  // namespace Org.WebRtc.Internal

#endif  // WEBRTC_BUILD_WINRT_GYP_API_WEBRTCMEDIASOURCE_H_
