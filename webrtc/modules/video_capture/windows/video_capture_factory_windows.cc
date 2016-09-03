/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifdef WINRT
#include "webrtc/modules/video_capture/windows/video_capture_winrt.h"
#else
#include "webrtc/modules/video_capture/windows/video_capture_ds.h"
#include "webrtc/modules/video_capture/windows/video_capture_mf.h"
#endif
#include "webrtc/system_wrappers/include/ref_count.h"

namespace webrtc {
namespace videocapturemodule {

// static
VideoCaptureModule::DeviceInfo* VideoCaptureImpl::CreateDeviceInfo(
    const int32_t id) {
#ifdef WINRT
  return DeviceInfoWinRT::Create(id).release();
#else
  // TODO(tommi): Use the Media Foundation version on Vista and up.
  return DeviceInfoDS::Create(id);
#endif
}

VideoCaptureModule* VideoCaptureImpl::Create(const int32_t id,
                                             const char* device_id) {
  if (device_id == NULL)
    return NULL;

#ifdef WINRT
  RefCountImpl<VideoCaptureWinRT>* capture =
    new RefCountImpl<VideoCaptureWinRT>(id);
  if (capture->Init(id, device_id) != 0) {
    delete capture;
    capture = NULL;
  }
  return capture;
#else
  // TODO(tommi): Use Media Foundation implementation for Vista and up.
  RefCountImpl<VideoCaptureDS>* capture = new RefCountImpl<VideoCaptureDS>(id);
  if (capture->Init(id, device_id) != 0) {
    delete capture;
    capture = NULL;
  }
  return capture;
#endif
}

}  // namespace videocapturemodule
}  // namespace webrtc
