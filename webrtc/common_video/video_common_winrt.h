/*
*  Copyright (c) 2016 Opticaltone. All Rights Reserved.
*
*  Use of this source code is governed by a BSD-style license
*  that can be found in the LICENSE file in the root of the source
*  tree. An additional intellectual property rights grant can be found
*  in the file PATENTS.  All contributing project authors may
*  be found in the AUTHORS file in the root of the source tree.
*/

#ifndef WEBRTC_MODULES_VIDEO_COMMON_WINRT_H_
#define WEBRTC_MODULES_VIDEO_COMMON_WINRT_H_

#include <functional>
#include <vector>
#include "webrtc/modules/video_capture/video_capture_impl.h"
#include "webrtc/modules/video_capture/windows/device_info_winrt.h"

namespace webrtc {
	class VideoCommonWinRT
	{
		private:
			static Windows::UI::Core::CoreDispatcher^ windowDispatcher;
		public:
			static void SetCoreDispatcher(Windows::UI::Core::CoreDispatcher^ inWindowDispatcher);
			static Windows::UI::Core::CoreDispatcher^ GetCoreDispatcher();

	};  // VideoCommonWinRT
}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_COMMON_WINRT_H_
