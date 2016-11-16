
// Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef WEBRTC_BUILD_WINRT_GYP_API_MEDIASOURCEHELPER_H_
#define WEBRTC_BUILD_WINRT_GYP_API_MEDIASOURCEHELPER_H_

#include "Media.h"
#include "webrtc/api/mediastreaminterface.h"
#include "webrtc/system_wrappers/include/tick_util.h"
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"

using Microsoft::WRL::ComPtr;
namespace Org {
	namespace WebRtc {
		/// <summary>
		/// Delegate used to notify about first video frame rendering.
		/// </summary>
		public delegate void FirstFrameRenderedEventHandler(double timestamp);

		public ref class FirstFrameRenderHelper sealed {
		public:
			/// <summary>
			/// Event fires when the first video frame renders.
			/// </summary>
			static event FirstFrameRenderedEventHandler^ FirstFrameRendered;
		internal:
			static void FireEvent(double timestamp);
		};
	}
}
namespace Org {
	namespace WebRtc {
		namespace Internal {

			struct SampleData {
				SampleData();
				ComPtr<IMFSample> sample;
				bool sizeHasChanged;
				SIZE size;
				bool rotationHasChanged;
				int rotation;
				LONGLONG renderTime;
			};

			class MediaSourceHelper {
			public:
				MediaSourceHelper(bool isH264,
					std::function<HRESULT(cricket::VideoFrame* frame, IMFSample** sample)> mkSample,
					std::function<void(int)> fpsCallback);
				~MediaSourceHelper();

				void SetStartTimeNow();
				void QueueFrame(cricket::VideoFrame* frame);
				rtc::scoped_ptr<SampleData> DequeueFrame();
				bool HasFrames();

			private:
				rtc::scoped_ptr<webrtc::CriticalSectionWrapper> _lock;
				std::list<cricket::VideoFrame*> _frames;
				bool _isFirstFrame;
				LONGLONG _startTime;
				// One peculiarity, the timestamp of a sample should be slightly
				// in the future for Media Foundation to handle it properly.
				int _futureOffsetMs;
				// We keep the last sample time to catch cases where samples are
				// requested so quickly that the sample time doesn't change.
				// We then increment it slightly to prevent giving MF duplicate times.
				LONGLONG _lastSampleTime;
				// Stored to detect changes.
				SIZE _lastSize;
				// In degrees.  In practice it can only be 0, 90, 180 or 270.
				int _lastRotation;

				rtc::scoped_ptr<SampleData> DequeueH264Frame();
				rtc::scoped_ptr<SampleData> DequeueI420Frame();


				// Gets the next timestamp using the clock.
				// Guarantees no duplicate timestamps.
				LONGLONG GetNextSampleTimeHns(LONGLONG frameRenderTime);

				void CheckForAttributeChanges(cricket::VideoFrame* frame, SampleData* data);

				std::function<HRESULT(cricket::VideoFrame* frame, IMFSample** sample)> _mkSample;
				std::function<void(int)> _fpsCallback;

				// Called whenever a new sample is sent for rendering.
				void UpdateFrameRate();
				// State related to calculating FPS.
				int _frameCounter;
				webrtc::TickTime _lastTimeFPSCalculated;

				// Are the frames H264 encoded.
				bool _isH264;

				webrtc::TickTime _startTickTime;
			};
		}
	}

}  // namespace Org.WebRtc.Internal

#endif  // WEBRTC_BUILD_WINRT_GYP_API_MEDIASOURCEHELPER_H_
