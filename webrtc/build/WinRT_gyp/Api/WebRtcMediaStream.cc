
// Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "WebRtcMediaStream.h"
#include "WebRtcMediaSource.h"
#include <mfapi.h>
#include <mferror.h>
#include <d3d11_2.h>
#include <d2d1_2.h>
#include <functional>
#include "webrtc/media/base/videoframe.h"
#include "RTMediaStreamSource.h"
#include "webrtc/api/videosourceinterface.h"
#include "libyuv/convert.h"

using Microsoft::WRL::MakeAndInitialize;
using Windows::System::Threading::TimerElapsedHandler;

namespace webrtc {
  namespace vcm {
    // How we trigger a key frame request when
    // registering an H264 renderer. We render
    // encoded samples so we have to request a
    // key frame as fast as possible otherwise
    // we don't render anything until the next
    // key frame.
    extern bool globalRequestKeyFrame;
  }
}

namespace Org {
	namespace WebRtc {
		namespace Internal {

#define MAX_FRAME_DELAY_MS 30
#define RETURN_ON_FAIL(code) { HRESULT hr = code; if (FAILED(hr)) {OutputDebugString(L"[Failed]\r\n"); return hr;} }

			// #define USE_MEMORY_BUFFERS

			class AutoFunction {
			public:
				explicit AutoFunction(std::function<void()> fn) : _fn(fn) {}
				~AutoFunction() { _fn(); }
			private:
				std::function<void()> _fn;
			};


			WebRtcMediaStream::WebRtcMediaStream() :
				_frameBufferIndex(0), _frameReady(0), _frameCount(0),
				_lock(webrtc::CriticalSectionWrapper::CreateCriticalSection()),
				_gpuVideoBuffer(false),
				_frameBeingQueued(0), _started(false) {
				_mediaBuffers.resize(BufferCount);
			}

			WebRtcMediaStream::~WebRtcMediaStream() {
				// To be safe.  Sometimes we get destroyed
				// without having been shutdown.
				Shutdown();
				// Wait until no frames are being queued
				// from the webrtc callback.
				while (_frameBeingQueued > 0) {
					Sleep(1);
				}
			}

			HRESULT WebRtcMediaStream::RuntimeClassInitialize(
				WebRtcMediaSource* source, MediaVideoTrack^ track, String^ id) {
				webrtc::CriticalSectionScoped csLock(_lock.get());
				if (_eventQueue != nullptr)
					return S_OK;

				_source = source;
				_track = track;
				_id = id;

				_isH264 = track->GetImpl()->GetSource()->IsH264Source();

				// Create the helper with the callback functions.
				_helper.reset(new MediaSourceHelper(_isH264,
					[this](cricket::VideoFrame* frame, IMFSample** sample) -> HRESULT {
					return MakeSampleCallback(frame, sample);
				},
					[this](int fps) {
					return FpsCallback(fps);
				}));

				RETURN_ON_FAIL(CreateMediaType(64, 64, 0, &_mediaType, _isH264));
				RETURN_ON_FAIL(MFCreateEventQueue(&_eventQueue));
				RETURN_ON_FAIL(MFCreateStreamDescriptor(1, 1, _mediaType.GetAddressOf(), &_streamDescriptor));
				ComPtr<IMFMediaTypeHandler> mediaTypeHandler;
				RETURN_ON_FAIL(_streamDescriptor->GetMediaTypeHandler(&mediaTypeHandler));
				RETURN_ON_FAIL(mediaTypeHandler->SetCurrentMediaType(_mediaType.Get()));

				track->SetRenderer(this);
				if (_isH264) {
					webrtc::vcm::globalRequestKeyFrame = true;
				}
				return S_OK;
			}

			// IMFMediaEventGenerator
			IFACEMETHODIMP WebRtcMediaStream::GetEvent(
				DWORD dwFlags, IMFMediaEvent **ppEvent) {
				webrtc::CriticalSectionScoped csLock(_lock.get());
				if (_eventQueue == nullptr) {
					return MF_E_SHUTDOWN;
				}
				return _eventQueue->GetEvent(dwFlags, ppEvent);
			}

			IFACEMETHODIMP WebRtcMediaStream::BeginGetEvent(
				IMFAsyncCallback *pCallback, IUnknown *punkState) {
				webrtc::CriticalSectionScoped csLock(_lock.get());
				if (_eventQueue == nullptr) {
					return MF_E_SHUTDOWN;
				}
				return _eventQueue->BeginGetEvent(pCallback, punkState);
			}

			IFACEMETHODIMP WebRtcMediaStream::EndGetEvent(
				IMFAsyncResult *pResult, IMFMediaEvent **ppEvent) {
				webrtc::CriticalSectionScoped csLock(_lock.get());
				if (_eventQueue == nullptr) {
					return MF_E_SHUTDOWN;
				}
				return _eventQueue->EndGetEvent(pResult, ppEvent);
			}

			IFACEMETHODIMP WebRtcMediaStream::QueueEvent(
				MediaEventType met, const GUID& guidExtendedType,
				HRESULT hrStatus, const PROPVARIANT *pvValue) {
				webrtc::CriticalSectionScoped csLock(_lock.get());
				if (_eventQueue == nullptr) {
					return MF_E_SHUTDOWN;
				}
				return _eventQueue->QueueEventParamVar(
					met, guidExtendedType, hrStatus, pvValue);
			}

			// IMFMediaStream
			IFACEMETHODIMP WebRtcMediaStream::GetMediaSource(
				IMFMediaSource **ppMediaSource) {
				webrtc::CriticalSectionScoped csLock(_lock.get());
				return _source->QueryInterface(IID_IMFMediaSource,
					reinterpret_cast<void**>(ppMediaSource));
			}

			IFACEMETHODIMP WebRtcMediaStream::GetStreamDescriptor(
				IMFStreamDescriptor **ppStreamDescriptor) {
				webrtc::CriticalSectionScoped csLock(_lock.get());
				*ppStreamDescriptor = _streamDescriptor.Get();
				(*ppStreamDescriptor)->AddRef();
				return S_OK;
			}

			IFACEMETHODIMP WebRtcMediaStream::RequestSample(IUnknown *pToken) {
				webrtc::CriticalSectionScoped csLock(_lock.get());
				if (_eventQueue == nullptr) {
					return E_POINTER;
				}
				InterlockedIncrement(&_frameReady);
				ReplyToSampleRequest();

				return S_OK;
			}

			IFACEMETHODIMP WebRtcMediaStream::GetService(
				REFGUID guidService, REFIID riid, LPVOID *ppvObject) {
				return MF_E_UNSUPPORTED_SERVICE;
			}

			void WebRtcMediaStream::RenderFrame(
				const cricket::VideoFrame *frame) {
				auto frameCopy = frame->Copy();
				InterlockedIncrement(&_frameBeingQueued);
				// Do the processing async because there's a risk of a deadlock otherwise.
				Concurrency::create_async([this, frameCopy] {
					{
						webrtc::CriticalSectionScoped csLock(_lock.get());
						if (_helper != nullptr) {
							_helper->QueueFrame(frameCopy);
							ReplyToSampleRequest();
						}
					}
					InterlockedDecrement(&_frameBeingQueued);
				});
			}

			HRESULT WebRtcMediaStream::CreateMediaType(
				unsigned int width, unsigned int height,
				unsigned int rotation, IMFMediaType** ppType, bool isH264) {
				// Create media type
				// Make sure the dimensions are even
				width &= ~((unsigned int)1);
				height &= ~((unsigned int)1);
				ComPtr<IMFMediaType> mediaType;
				RETURN_ON_FAIL(MFCreateMediaType(&mediaType));
				RETURN_ON_FAIL(mediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));

				if (isH264) {
					RETURN_ON_FAIL(mediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264));
				}
				else {
					RETURN_ON_FAIL(mediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12));
					RETURN_ON_FAIL(mediaType->SetUINT32(MF_MT_FIXED_SIZE_SAMPLES, TRUE));
					RETURN_ON_FAIL(mediaType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE));
					RETURN_ON_FAIL(mediaType->SetUINT32(MF_MT_SAMPLE_SIZE, width * height * 3 / 2));
					RETURN_ON_FAIL(mediaType->SetUINT32(MF_MT_DEFAULT_STRIDE, width));
				}

				RETURN_ON_FAIL(MFSetAttributeSize(mediaType.Get(), MF_MT_FRAME_SIZE, width, height));
				RETURN_ON_FAIL(mediaType->SetUINT32(MF_LOW_LATENCY, TRUE));
				RETURN_ON_FAIL(mediaType->SetUINT32(MF_MT_VIDEO_ROTATION, rotation));
				RETURN_ON_FAIL(mediaType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive));
				*ppType = mediaType.Detach();
				return S_OK;
			}

			HRESULT WebRtcMediaStream::MakeSampleCallback(
				const cricket::VideoFrame* frame, IMFSample** sample) {
				ComPtr<IMFSample> spSample;
				RETURN_ON_FAIL(MFCreateSample(&spSample));

				// Make sure the destination buffer in even. Crop one pixel if odd.
				unsigned int destWidth = (unsigned int)(frame->GetWidth() & (~((size_t)1)));
				unsigned int destHeight = (unsigned int)(frame->GetHeight() & (~((size_t)1)));
				// Make sure the buffers are the right size
				{
					unsigned int width, height;
					RETURN_ON_FAIL(MFGetAttributeSize(
						_mediaType.Get(), MF_MT_FRAME_SIZE, &width, &height));
					if (destWidth != width || destHeight != height) {
						RETURN_ON_FAIL(CreateMediaType(destWidth, destHeight,
							(unsigned int)frame->GetVideoRotation(), &_mediaType,
							_isH264));
						ResetMediaBuffers();
					}
				}

				// Pick the next buffer.
				ComPtr<IMFMediaBuffer> buffer;
				buffer = _mediaBuffers[_frameBufferIndex];
				if (buffer == nullptr)
					return E_FAIL;
				_frameBufferIndex++;
				_frameBufferIndex %= BufferCount;

				RETURN_ON_FAIL(spSample->AddBuffer(buffer.Get()));

				ComPtr<IMF2DBuffer2> buffer2d;
				RETURN_ON_FAIL(buffer.As(&buffer2d));

				BYTE* destRawData;
				BYTE* bufferStart;
				LONG pitch;
				DWORD destMediaBufferSize;

				RETURN_ON_FAIL(buffer2d->Lock2DSize(MF2DBuffer_LockFlags_Write,
					&destRawData, &pitch, &bufferStart, &destMediaBufferSize));
				AutoFunction autoUnlockBuffer([buffer2d]() {buffer2d->Unlock2D(); });

				// Convert to NV12
				uint8* uvDest = destRawData + (pitch * destHeight);
				libyuv::I420ToNV12(frame->GetYPlane(), frame->GetYPitch(),
					frame->GetUPlane(), frame->GetUPitch(),
					frame->GetVPlane(), frame->GetVPitch(),
					reinterpret_cast<uint8*>(destRawData), pitch,
					uvDest, pitch,
					static_cast<int>(destWidth),
					static_cast<int>(destHeight));

				*sample = spSample.Detach();
				return S_OK;
			}

			void WebRtcMediaStream::FpsCallback(int fps) {
				Concurrency::create_async([this, fps] {
					Org::WebRtc::FrameCounterHelper::FireEvent(
						_id, fps.ToString());
				});
			}

			HRESULT WebRtcMediaStream::ReplyToSampleRequest() {
				webrtc::CriticalSectionScoped csLock(_lock.get());
				if (_frameReady == 0 || !_helper->HasFrames()) {
					return S_OK;
				}

				_frameCount++;

				auto sampleData = _helper->DequeueFrame();

				if (sampleData == nullptr) {
					return S_OK;
				}

				// Update rotation property
				if (sampleData->rotationHasChanged || sampleData->sizeHasChanged) {
					unsigned int width, height;
					if (sampleData->sizeHasChanged) {
						width = sampleData->size.cx;
						height = sampleData->size.cy;
					}
					else {
						RETURN_ON_FAIL(MFGetAttributeSize(
							_mediaType.Get(), MF_MT_FRAME_SIZE, &width, &height));
					}

					unsigned int rotation;
					if (sampleData->rotationHasChanged) {
						rotation = sampleData->rotation;
					}
					else {
						RETURN_ON_FAIL(_mediaType->GetUINT32(MF_MT_VIDEO_ROTATION, &rotation));
					}

					if (_isH264)
						OutputDebugString((L"Frame format changed: "
							+ width.ToString() + L"x" + height.ToString()
							+ " rotation:" + rotation.ToString() + L"\r\n")->Data());

					CreateMediaType(width, height, rotation, &_mediaType, _isH264);
					_eventQueue->QueueEventParamUnk(MEStreamFormatChanged,
						GUID_NULL, S_OK, _mediaType.Get());
					ResetMediaBuffers();

					Org::WebRtc::ResolutionHelper::FireEvent(
						_id, width, height);
				}

				_eventQueue->QueueEventParamUnk(MEMediaSample,
					GUID_NULL, S_OK, sampleData->sample.Get());

				InterlockedDecrement(&_frameReady);
				return S_OK;
			}

			STDMETHODIMP WebRtcMediaStream::Start(
				IMFPresentationDescriptor *pPresentationDescriptor,
				const GUID *pguidTimeFormat, const PROPVARIANT *pvarStartPosition) {
				webrtc::CriticalSectionScoped csLock(_lock.get());
				if (_eventQueue == nullptr) {
					return MF_E_SHUTDOWN;
				}
				// Start stream
				RETURN_ON_FAIL(QueueEvent(MEStreamStarted, GUID_NULL, S_OK, pvarStartPosition));

				if (!_started) {
					_helper->SetStartTimeNow();
					_started = true;
				}
				return S_OK;
			}

			STDMETHODIMP WebRtcMediaStream::Stop() {
				webrtc::CriticalSectionScoped csLock(_lock.get());
				if (_eventQueue == nullptr) {
					return MF_E_SHUTDOWN;
				}
				RETURN_ON_FAIL(QueueEvent(MEStreamStopped, GUID_NULL, S_OK, nullptr));
				return S_OK;
			}

			STDMETHODIMP WebRtcMediaStream::Shutdown() {
				webrtc::CriticalSectionScoped csLock(_lock.get());

				_track->UnsetRenderer(this);
				if (_eventQueue != nullptr) {
					_eventQueue->Shutdown();
				}

				_deviceManager = nullptr;
				_eventQueue = nullptr;

				_helper.reset();
				return S_OK;
			}

			STDMETHODIMP WebRtcMediaStream::SetD3DManager(
				ComPtr<IMFDXGIDeviceManager> manager) {
				webrtc::CriticalSectionScoped csLock(_lock.get());
				_deviceManager = manager;
				HANDLE deviceHandle;
				_deviceManager->OpenDeviceHandle(&deviceHandle);
				AutoFunction autoCloseHandle([this, deviceHandle]() {
					_deviceManager->CloseDeviceHandle(deviceHandle); });
				ComPtr<ID3D11Device> device;
				_deviceManager->LockDevice(deviceHandle, IID_ID3D11Device, &device, TRUE);
				AutoFunction autoUnlockDevice([this, deviceHandle]() {
					_deviceManager->UnlockDevice(deviceHandle, TRUE); });
				if ((unsigned int)device->GetFeatureLevel() >=
					(unsigned int)D3D_FEATURE_LEVEL_11_1) {
					_gpuVideoBuffer = true;
					OutputDebugString(L"DirectX 11.1 or greater detected, using GPU video render buffers\r\n");
				}
				else {
					_gpuVideoBuffer = false;
					OutputDebugString(L"GPU video render buffers are NOT supported\r\n");
				}
				_deviceManager->UnlockDevice(deviceHandle, FALSE);
				ResetMediaBuffers();
				return S_OK;
			}

			HRESULT WebRtcMediaStream::ResetMediaBuffers() {
				for (auto&& buffer : _mediaBuffers) {
					buffer.Reset();
				}

				unsigned int width, height;
				RETURN_ON_FAIL(MFGetAttributeSize(_mediaType.Get(),
					MF_MT_FRAME_SIZE, &width, &height));

				if (_deviceManager == nullptr) {
					return S_OK;
				}
				if (_gpuVideoBuffer) {
					HANDLE hDevice = NULL;
					ComPtr<ID3D11Device> device;
					RETURN_ON_FAIL(_deviceManager->OpenDeviceHandle(&hDevice));
					AutoFunction autoCloseHandle([this, hDevice]() {
						_deviceManager->CloseDeviceHandle(hDevice); });

					RETURN_ON_FAIL(_deviceManager->LockDevice(hDevice, IID_ID3D11Device, &device, TRUE));
					AutoFunction autoUnlockDevice([this, hDevice]() {
						_deviceManager->UnlockDevice(hDevice, TRUE); });

					for (auto&& buffer : _mediaBuffers) {
						D3D11_TEXTURE2D_DESC texDesc;
						ComPtr<ID3D11Texture2D> frameTexture;
						ZeroMemory(&texDesc, sizeof(texDesc));
						texDesc.Width = width;
						texDesc.Height = height;
						texDesc.MipLevels = 1;
						texDesc.ArraySize = 1;
						texDesc.Format = DXGI_FORMAT_NV12;
						texDesc.SampleDesc.Count = 1;
						texDesc.Usage = D3D11_USAGE_DEFAULT;
						if (FAILED(device->CreateTexture2D(&texDesc, nullptr,
							frameTexture.ReleaseAndGetAddressOf()))) {
							_gpuVideoBuffer = false;
							OutputDebugString(
								L"Failed to create DirectX 2D texture for video buffers, falling back to main memory buffers\r\n");
							return ResetMediaBuffers();
						}
						RETURN_ON_FAIL(MFCreateDXGISurfaceBuffer(__uuidof(ID3D11Texture2D),
							frameTexture.Get(), 0, FALSE, &buffer));
					}
				}
				else {
					for (auto&& buffer : _mediaBuffers) {
						MFCreate2DMediaBuffer(
							width, height,
							MFVideoFormat_NV12.Data1, FALSE,
							&buffer);
					}
				}
				return S_OK;
			}
		}
	}
}  // namespace Org.WebRtc.Internal
