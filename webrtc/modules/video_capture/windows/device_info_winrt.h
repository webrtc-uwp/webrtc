/*
*  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
*
*  Use of this source code is governed by a BSD-style license
*  that can be found in the LICENSE file in the root of the source
*  tree. An additional intellectual property rights grant can be found
*  in the file PATENTS.  All contributing project authors may
*  be found in the AUTHORS file in the root of the source tree.
*/

#ifndef WEBRTC_MODULES_VIDEO_CAPTURE_WINDOWS_DEVICE_INFO_WINRT_H_
#define WEBRTC_MODULES_VIDEO_CAPTURE_WINDOWS_DEVICE_INFO_WINRT_H_

#include "webrtc/modules/video_capture/device_info_impl.h"

#include <ppltasks.h>

#include <map>

#include "webrtc/system_wrappers/include/critical_section_wrapper.h"
#include "webrtc/base/scoped_ptr.h"

namespace webrtc {
namespace videocapturemodule {

private ref class MediaCaptureDevicesWinRT sealed {
 private:
  MediaCaptureDevicesWinRT();

 public:
  virtual ~MediaCaptureDevicesWinRT();

  static MediaCaptureDevicesWinRT^ Instance();

  void ClearCaptureDevicesCache();
                                                                                                                                                                                                                                                                                                    
 internal:
  Platform::Agile<Windows::Media::Capture::MediaCapture>
    GetMediaCapture(Platform::String^ device_id);
  void RemoveMediaCapture(Platform::String^ device_id);

 private:
  std::map<Platform::String^,
           Platform::Agile<Windows::Media::Capture::MediaCapture> >
    media_capture_map_;
  CriticalSectionWrapper* critical_section_;
};

class DeviceInfoWinRT : public DeviceInfoImpl {
 public:
  // Factory function.
  static rtc::scoped_ptr<DeviceInfoWinRT> Create(const int32_t id);

  explicit DeviceInfoWinRT(const int32_t id);
  virtual ~DeviceInfoWinRT();

  virtual uint32_t NumberOfDevices();

  virtual int32_t GetDeviceName(uint32_t device_number,
                                char* device_name_utf8,
                                uint32_t device_name_utf8_length,
                                char* device_unique_id_utf8,
                                uint32_t device_unique_id_utf8_length,
                                char* product_unique_id_utf8,
                                uint32_t product_unique_id_utf8_length);

  virtual int32_t DisplayCaptureSettingsDialogBox(
    const char* device_unique_id_utf8,
    const char* dialog_title_utf8,
    void* parent_window,
    uint32_t position_x,
    uint32_t position_y);

 protected:
  // No-op Init
  int32_t Init() { return 0; }
  int32_t GetDeviceInfo(uint32_t device_number,
                        char* device_name_utf8,
                        uint32_t device_name_utf8_length,
                        char* device_unique_id_utf8,
                        uint32_t device_unique_id_utf8_length,
                        char* product_unique_id_utf8,
                        uint32_t product_unique_id_utf8_length);

  virtual int32_t
    CreateCapabilityMap(const char* device_unique_id_utf8);
};

}  // namespace videocapturemodule
}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CAPTURE_WINDOWS_DEVICE_INFO_WINRT_H_
