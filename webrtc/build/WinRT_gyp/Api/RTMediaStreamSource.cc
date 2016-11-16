// Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "webrtc/build/WinRT_gyp/Api/RTMediaStreamSource.h"
#include <mfapi.h>
#include <ppltasks.h>
#include <mfidl.h>
#include "webrtc/media/base/videoframe.h"
#include "webrtc/api/videosourceinterface.h"
#include "libyuv/convert.h"
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"
#include "webrtc/common_video/video_common_winrt.h"

using Microsoft::WRL::ComPtr;
using Platform::Collections::Vector;
using Windows::Media::Core::VideoStreamDescriptor;
using Windows::Media::Core::MediaStreamSourceSampleRequestedEventArgs;
using Windows::Media::Core::MediaStreamSourceSampleRequest;
using Windows::Media::Core::MediaStreamSourceStartingEventArgs;
using Windows::Media::Core::MediaStreamSource;
using Windows::Media::MediaProperties::VideoEncodingProperties;
using Windows::Media::MediaProperties::MediaEncodingSubtypes;
using Windows::System::Threading::TimerElapsedHandler;
using Windows::System::Threading::ThreadPoolTimer;

namespace Org {
	namespace WebRtc {
		namespace Internal {

			MediaStreamSource^ RTMediaStreamSource::CreateMediaSource(
				MediaVideoTrack^ track, uint32 frameRate, String^ id) {

				bool isH264 = track->GetImpl()->GetSource()->IsH264Source();

				auto streamState = ref new RTMediaStreamSource(track, isH264);
				streamState->_id = id;
				streamState->_idUtf8 = rtc::ToUtf8(streamState->_id->Data());
				streamState->_rtcRenderer = rtc::scoped_ptr<RTCRenderer>(
					new RTCRenderer(streamState));
				track->SetRenderer(streamState->_rtcRenderer.get());
				VideoEncodingProperties^ videoProperties;
				if (isH264) {
					videoProperties = VideoEncodingProperties::CreateH264();
					//videoProperties->ProfileId = Windows::Media::MediaProperties::H264ProfileIds::Baseline;
				}
				else {
					videoProperties =
						VideoEncodingProperties::CreateUncompressed(
							MediaEncodingSubtypes::Nv12, 10, 10);
				}
				streamState->_videoDesc = ref new VideoStreamDescriptor(videoProperties);

				// initial value, this will be override by incoming frame from webrtc.
				// this is needed since the UI element might request sample before webrtc has
				// incoming frame ready(ex.: remote stream), in this case, this initial value
				// will make sure we will at least create a small dummy frame.
				streamState->_videoDesc->EncodingProperties->Width = 720;
				streamState->_videoDesc->EncodingProperties->Height = 1280;

				Org::WebRtc::ResolutionHelper::FireEvent(id,
					streamState->_videoDesc->EncodingProperties->Width,
					streamState->_videoDesc->EncodingProperties->Height);

				streamState->_videoDesc->EncodingProperties->FrameRate->Numerator =
					frameRate;
				streamState->_videoDesc->EncodingProperties->FrameRate->Denominator = 1;
				auto streamSource = ref new MediaStreamSource(streamState->_videoDesc);

				auto startingCookie = streamSource->Starting +=
					ref new Windows::Foundation::TypedEventHandler<
					MediaStreamSource ^,
					MediaStreamSourceStartingEventArgs ^>([streamState](
						MediaStreamSource^ sender,
						MediaStreamSourceStartingEventArgs^ args) {
					// Get a deferall on the starting event so we can trigger it
					// when the first frame arrives.
					streamState->_startingDeferral = args->Request->GetDeferral();
				});

				streamState->_mediaStreamSource = streamSource;

				// Use a lambda to capture a strong reference to RTMediaStreamSource.
				// This is the only way to tie the lifetime of the RTMediaStreamSource
				// to that of the MediaStreamSource.
				auto sampleRequestedCookie = streamSource->SampleRequested +=
					ref new Windows::Foundation::TypedEventHandler<
					MediaStreamSource^,
					MediaStreamSourceSampleRequestedEventArgs^>([streamState](
						MediaStreamSource^ sender,
						MediaStreamSourceSampleRequestedEventArgs^ args) {
					streamState->OnSampleRequested(sender, args);
				});
				streamSource->Closed +=
					ref new Windows::Foundation::TypedEventHandler<
					Windows::Media::Core::MediaStreamSource^,
					Windows::Media::Core::MediaStreamSourceClosedEventArgs ^>(
						[streamState, startingCookie, sampleRequestedCookie](
							Windows::Media::Core::MediaStreamSource^ sender,
							Windows::Media::Core::MediaStreamSourceClosedEventArgs^ args) {
					LOG(LS_INFO) << "RTMediaStreamSource::OnClosed";
					streamState->Teardown();
					sender->Starting -= startingCookie;
					sender->SampleRequested -= sampleRequestedCookie;
				});

				// Create a timer which sends request progress periodically.
				{
					auto handler = ref new TimerElapsedHandler(streamState,
						&RTMediaStreamSource::ProgressTimerElapsedExecute);
					auto timespan = Windows::Foundation::TimeSpan();
					timespan.Duration = 500 * 1000 * 10;  // 500 ms in hns
					streamState->_progressTimer = ThreadPoolTimer::CreatePeriodicTimer(
						handler, timespan);
				}

				// Create a timer which ensures we don't display frames faster that expected.
				// Required because Media Foundation sometimes requests samples in burst mode
				// but we use the wall clock to drive timestamps.
				{
					auto handler = ref new TimerElapsedHandler(streamState,
						&RTMediaStreamSource::FPSTimerElapsedExecute);
					auto timespan = Windows::Foundation::TimeSpan();
					timespan.Duration = 15 * 1000 * 10;
					streamState->_fpsTimer = ThreadPoolTimer::CreatePeriodicTimer(handler,
						timespan);
				}

				return streamSource;
			}

			RTMediaStreamSource::RTMediaStreamSource(MediaVideoTrack^ videoTrack,
				bool isH264) :
				_videoTrack(videoTrack),
				_lock(webrtc::CriticalSectionWrapper::CreateCriticalSection()),
				_frameSentThisTime(false),
				_frameBeingQueued(0) {
				LOG(LS_INFO) << "RTMediaStreamSource::RTMediaStreamSource";

				// Create the helper with the callback functions.
				_helper.reset(new MediaSourceHelper(isH264,
					[this](cricket::VideoFrame* frame, IMFSample** sample) -> HRESULT {
					return MakeSampleCallback(frame, sample);
				},
					[this](int fps) {
					return FpsCallback(fps);
				}));
			}

			RTMediaStreamSource::~RTMediaStreamSource() {
				LOG(LS_INFO) << "RTMediaStreamSource::~RTMediaStreamSource ID=" << _idUtf8;
				Teardown();
			}

			void RTMediaStreamSource::Teardown() {
				LOG(LS_INFO) << "RTMediaStreamSource::Teardown() ID=" << _idUtf8;
				{
					webrtc::CriticalSectionScoped csLock(_lock.get());
					if (_progressTimer != nullptr) {
						_progressTimer->Cancel();
						_progressTimer = nullptr;
					}
					if (_fpsTimer != nullptr) {
						_fpsTimer->Cancel();
						_fpsTimer = nullptr;
					}
					if (_rtcRenderer != nullptr && _videoTrack != nullptr) {
						_videoTrack->UnsetRenderer(_rtcRenderer.get());
					}

					_videoTrack = nullptr;

					_request = nullptr;
					if (_deferral != nullptr) {
						_deferral->Complete();
						_deferral = nullptr;
					}
					if (_startingDeferral != nullptr) {
						_startingDeferral->Complete();
						_startingDeferral = nullptr;
					}
					_helper.reset();
				}

				// Wait until no frames are being queued
				// from the webrtc callback.
				while (_frameBeingQueued > 0) {
					Sleep(1);
				}

				{
					webrtc::CriticalSectionScoped csLock(_lock.get());
					if (_rtcRenderer != nullptr) {
						_rtcRenderer.reset();
					}
				}
				LOG(LS_INFO) << "RTMediaStreamSource::Teardown() done ID=" << _idUtf8;
			}

			RTMediaStreamSource::RTCRenderer::RTCRenderer(
				RTMediaStreamSource^ streamSource) : _streamSource(streamSource) {
			}

			RTMediaStreamSource::RTCRenderer::~RTCRenderer() {
				LOG(LS_INFO) << "RTMediaStreamSource::RTCRenderer::~RTCRenderer";
			}

			void RTMediaStreamSource::RTCRenderer::SetSize(
				uint32 width, uint32 height, uint32 reserved) {
				auto stream = _streamSource.Resolve<RTMediaStreamSource>();
				if (stream != nullptr) {
					stream->ResizeSource(width, height);
				}
			}

			void RTMediaStreamSource::RTCRenderer::RenderFrame(
				const cricket::VideoFrame *frame) {
				auto stream = _streamSource.Resolve<RTMediaStreamSource>();
				if (stream == nullptr) {
					LOG(LS_WARNING) << "RTCRenderer::RenderFrame: associated stream is null";
					return;
				}
				InterlockedIncrement(&stream->_frameBeingQueued);
				auto frameCopy = frame->Copy();
				// Do the processing async because there's a risk of a deadlock otherwise.
				Concurrency::create_async([this, frameCopy, stream] {
					stream->ProcessReceivedFrame(frameCopy);
					InterlockedDecrement(&stream->_frameBeingQueued);
				});
			}

			void RTMediaStreamSource::ProgressTimerElapsedExecute(ThreadPoolTimer^ source) {
				webrtc::CriticalSectionScoped csLock(_lock.get());
				if (_request != nullptr) {
					_request->ReportSampleProgress(1);
				}
			}

			void RTMediaStreamSource::FPSTimerElapsedExecute(ThreadPoolTimer^ source) {
				webrtc::CriticalSectionScoped csLock(_lock.get());
				_frameSentThisTime = false;
				if (_request != nullptr) {
					ReplyToSampleRequest();
				}
			}

			void RTMediaStreamSource::ReplyToSampleRequest() {
				auto sampleData = _helper->DequeueFrame();
				if (sampleData == nullptr) {
					return;
				}

				// Update rotation property
				if (sampleData->rotationHasChanged) {
					auto props = _videoDesc->EncodingProperties->Properties;
					OutputDebugString((L"Video rotation changed: " + sampleData->rotation + "\r\n")->Data());
					props->Insert(MF_MT_VIDEO_ROTATION, sampleData->rotation);
				}

				// Frame size in EncodingProperties needs to be updated before completing
				// deferral, otherwise the MediaElement will receive a frame having different
				// size and application may crash.
				if (sampleData->sizeHasChanged) {
					auto props = _videoDesc->EncodingProperties;
					props->Width = (unsigned int)sampleData->size.cx;
					props->Height = (unsigned int)sampleData->size.cy;
					Org::WebRtc::ResolutionHelper::FireEvent(
						_id, props->Width, props->Height);
					OutputDebugString((L"Video frame size changed for " + _id +
						L" W=" + props->Width +
						L" H=" + props->Height + L"\r\n")->Data());
				}

				Microsoft::WRL::ComPtr<IMFMediaStreamSourceSampleRequest> spRequest;
				HRESULT hr = reinterpret_cast<IInspectable*>(_request)->QueryInterface(
					spRequest.ReleaseAndGetAddressOf());

				//Fixme: it appears that MSS works well if we hardcode the sample duration with 33ms,
				// Don' know why, override the value set by the mediahelper
				LONGLONG duration = (LONGLONG)((1.0 / 30) * 1000 * 1000 * 10);
				sampleData->sample.Get()->SetSampleDuration(duration);

				hr = spRequest->SetSample(sampleData->sample.Get());

				if (_deferral != nullptr) {
					_deferral->Complete();
				}

				_frameSentThisTime = true;

				_request = nullptr;
				_deferral = nullptr;
			}

			HRESULT RTMediaStreamSource::MakeSampleCallback(
				cricket::VideoFrame* frame, IMFSample** sample) {
				ComPtr<IMFSample> spSample;
				HRESULT hr = MFCreateSample(spSample.GetAddressOf());
				if (FAILED(hr)) {
					return E_FAIL;
				}
				ComPtr<IMFMediaBuffer> mediaBuffer;
				hr = MFCreate2DMediaBuffer(
					(DWORD)frame->GetWidth(), (DWORD)frame->GetHeight(),
					cricket::FOURCC_NV12, FALSE,
					mediaBuffer.GetAddressOf());
				if (FAILED(hr)) {
					return E_FAIL;
				}

				spSample->AddBuffer(mediaBuffer.Get());

				ComPtr<IMF2DBuffer2> imageBuffer;
				if (FAILED(mediaBuffer.As(&imageBuffer))) {
					return E_FAIL;
				}

				BYTE* destRawData;
				BYTE* buffer;
				LONG pitch;
				DWORD destMediaBufferSize;

				if (FAILED(imageBuffer->Lock2DSize(MF2DBuffer_LockFlags_Write,
					&destRawData, &pitch, &buffer, &destMediaBufferSize))) {
					return E_FAIL;
				}
				try {
					frame->MakeExclusive();
					// Convert to NV12
					uint8* uvDest = destRawData + (pitch * frame->GetHeight());
					libyuv::I420ToNV12(frame->GetYPlane(), frame->GetYPitch(),
						frame->GetUPlane(), frame->GetUPitch(),
						frame->GetVPlane(), frame->GetVPitch(),
						reinterpret_cast<uint8*>(destRawData), pitch,
						uvDest, pitch,
						static_cast<int>(frame->GetWidth()),
						static_cast<int>(frame->GetHeight()));
				}
				catch (...) {
					LOG(LS_ERROR) << "Exception caught in RTMediaStreamSource::ConvertFrame()";
				}
				imageBuffer->Unlock2D();

				*sample = spSample.Detach();
				return S_OK;
			}

			void RTMediaStreamSource::FpsCallback(int fps) {
				Org::WebRtc::FrameCounterHelper::FireEvent(
					_id, fps.ToString());
			}

			void RTMediaStreamSource::OnSampleRequested(
				MediaStreamSource ^sender, MediaStreamSourceSampleRequestedEventArgs ^args) {
				try {
					// Check to detect cases where samples are still being requested
					// but the source has ended.
					auto trackState = _videoTrack->GetImpl()->GetSource()->state();
					if (trackState == webrtc::MediaSourceInterface::kEnded) {
						return;
					}
					if (_mediaStreamSource == nullptr)
						return;

					webrtc::CriticalSectionScoped csLock(_lock.get());

					_request = args->Request;
					if (_request == nullptr) {
						return;
					}
					if (_helper == nullptr) {  // may be null while tearing down
						return;
					}

					if (!_frameSentThisTime && _helper->HasFrames()) {
						ReplyToSampleRequest();
						return;
					}
					else {
						// Save the request and referral for when a sample comes in.
						if (_deferral != nullptr) {
							LOG(LS_ERROR) << "Got deferral when another hasn't completed.";
						}
						_deferral = _request->GetDeferral();
						return;
					}
				}
				catch (...) {
					LOG(LS_ERROR) << "Exception in RTMediaStreamSource::OnSampleRequested.";
				}
			}

			void RTMediaStreamSource::ProcessReceivedFrame(
				cricket::VideoFrame *frame) {
				webrtc::CriticalSectionScoped csLock(_lock.get());

				if (_startingDeferral != nullptr) {
					_startingDeferral->Complete();
					_startingDeferral = nullptr;
				}

				if (_helper == nullptr) {  // May be null while tearing down the MSS
					return;
				}
				_helper->QueueFrame(frame);

				// If we have a pending request, reply to it now.
				if (_deferral != nullptr && _request != nullptr && !_frameSentThisTime) {
					ReplyToSampleRequest();
				}
			}

			void RTMediaStreamSource::ResizeSource(uint32 width, uint32 height) {
			}
		}
	}
}  // namespace Org.WebRtc.Internal


void Org::WebRtc::FrameCounterHelper::FireEvent(String^ id,
  Platform::String^ str) {
  Windows::UI::Core::CoreDispatcher^ _windowDispatcher =	VideoCommonWinRT::GetCoreDispatcher();
  if (_windowDispatcher != nullptr) {
    _windowDispatcher->RunAsync(
      Windows::UI::Core::CoreDispatcherPriority::Normal,
      ref new Windows::UI::Core::DispatchedHandler([id, str] {
      FramesPerSecondChanged(id, str);
    }));
  } else {
    FramesPerSecondChanged(id, str);
  }
}

void Org::WebRtc::ResolutionHelper::FireEvent(String^ id,
  unsigned int width, unsigned int heigth) {
  Windows::UI::Core::CoreDispatcher^ _windowDispatcher =	VideoCommonWinRT::GetCoreDispatcher();
  if (_windowDispatcher != nullptr) {
    _windowDispatcher->RunAsync(
      Windows::UI::Core::CoreDispatcherPriority::Normal,
      ref new Windows::UI::Core::DispatchedHandler([id, width, heigth] {
      ResolutionChanged(id, width, heigth);
    }));
  } else {
    ResolutionChanged(id, width, heigth);
  }
}
