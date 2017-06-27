/*
*  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
*
*  Use of this source code is governed by a BSD-style license
*  that can be found in the LICENSE file in the root of the source
*  tree. An additional intellectual property rights grant can be found
*  in the file PATENTS.  All contributing project authors may
*  be found in the AUTHORS file in the root of the source tree.
*/

#ifndef WEBRTC_MODULES_VIDEO_CAPTURE_WINDOWS_VIDEO_CAPTURE_WINUWP_H_
#define WEBRTC_MODULES_VIDEO_CAPTURE_WINDOWS_VIDEO_CAPTURE_WINUWP_H_

#include <functional>
#include <vector>
#include "webrtc/modules/video_capture/video_capture_impl.h"
#include "webrtc/modules/video_capture/windows/device_info_winuwp.h"

namespace webrtc {
namespace videocapturemodule {

ref class CaptureDevice;
ref class BlackFramesGenerator;
ref class DisplayOrientation;

class CaptureDeviceListener {
 public:
  virtual void OnIncomingFrame(uint8_t* video_frame,
                               size_t video_frame_length,
                               const VideoCaptureCapability& frame_info) = 0;
  virtual void OnCaptureDeviceFailed(HRESULT code,
    Platform::String^ message) = 0;
};

class DisplayOrientationListener {
 public:
  virtual void OnDisplayOrientationChanged(
    Windows::Graphics::Display::DisplayOrientations orientation) = 0;
};

class AppStateObserver {
 public:
  virtual void DisplayOrientationChanged(
    Windows::Graphics::Display::DisplayOrientations display_orientation) = 0;
};

class AppStateDispatcher : public AppStateObserver {
 public:
  static AppStateDispatcher* Instance();

  void DisplayOrientationChanged(
    Windows::Graphics::Display::DisplayOrientations display_orientation);
  Windows::Graphics::Display::DisplayOrientations GetOrientation() const;
  void AddObserver(AppStateObserver* observer);
  void RemoveObserver(AppStateObserver* observer);

 private:
  AppStateDispatcher();

  std::vector<AppStateObserver*> observers;
  static AppStateDispatcher* instance_;
  Windows::Graphics::Display::DisplayOrientations display_orientation_;
};

class VideoCaptureWinUWP
    : public VideoCaptureImpl,
      public CaptureDeviceListener,
      public AppStateObserver,
      public DisplayOrientationListener {
 public:
  explicit VideoCaptureWinUWP();

  int32_t Init(const char* device_id);

  // Overrides from VideoCaptureImpl.
  virtual int32_t StartCapture(const VideoCaptureCapability& capability);
  virtual int32_t StopCapture();
  virtual bool CaptureStarted();
  virtual int32_t CaptureSettings(VideoCaptureCapability& settings);

  virtual bool SuspendCapture();
  virtual bool ResumeCapture();
  virtual bool IsSuspended();

  // Overrides from AppStateObserver
  void DisplayOrientationChanged(
    Windows::Graphics::Display::DisplayOrientations display_orientation) override;

  // Overrides from DisplayOrientationListener
  void OnDisplayOrientationChanged(
    Windows::Graphics::Display::DisplayOrientations orientation) override;

 protected:
  virtual ~VideoCaptureWinUWP();

  virtual void OnIncomingFrame(uint8_t* video_frame,
                               size_t video_frame_length,
                               const VideoCaptureCapability& frame_info);

  virtual void OnCaptureDeviceFailed(HRESULT code,
                                     Platform::String^ message);

  virtual void ApplyDisplayOrientation(
    Windows::Graphics::Display::DisplayOrientations orientation);

 private:
  Platform::String^ device_id_;
  CaptureDevice^ device_;
  Windows::Devices::Enumeration::Panel camera_location_;
  DisplayOrientation^ display_orientation_;
  BlackFramesGenerator^ fake_device_;
  VideoCaptureCapability last_frame_info_;
  Windows::Media::MediaProperties::IVideoEncodingProperties^
    video_encoding_properties_;
  Windows::Media::MediaProperties::MediaEncodingProfile^
    media_encoding_profile_;
};

// Helper function to run code on the WinUWP CoreDispatcher
// and only return once the call completed.
void RunOnCoreDispatcher(std::function<void()> fn, bool async = false);

}  // namespace videocapturemodule
}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CAPTURE_WINDOWS_VIDEO_CAPTURE_WINUWP_H_
