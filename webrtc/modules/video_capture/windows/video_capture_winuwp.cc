/*
*  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
*
*  Use of this source code is governed by a BSD-style license
*  that can be found in the LICENSE file in the root of the source
*  tree. An additional intellectual property rights grant can be found
*  in the file PATENTS.  All contributing project authors may
*  be found in the AUTHORS file in the root of the source tree.
*/

#include "webrtc/modules/video_capture/windows/video_capture_winuwp.h"

#include <ppltasks.h>

#include <string>
#include <vector>
#include <Mferror.h>

#include "webrtc/system_wrappers/include/logging.h"
#include "webrtc/system_wrappers/include/event_wrapper.h"
#include "webrtc/modules/video_capture/windows/video_capture_sink_winuwp.h"
#include "webrtc/base/Win32.h"
#include "libyuv/planar_functions.h"
#include "webrtc/common_video/video_common_winuwp.h"

using Microsoft::WRL::ComPtr;
using Windows::Devices::Enumeration::DeviceClass;
using Windows::Devices::Enumeration::DeviceInformation;
using Windows::Devices::Enumeration::DeviceInformationCollection;
using Windows::Devices::Enumeration::Panel;
using Windows::Graphics::Display::DisplayInformation;
using Windows::Graphics::Display::DisplayOrientations;

using Windows::Media::Capture::MediaCapture;
using Windows::Media::Capture::MediaCaptureFailedEventArgs;
using Windows::Media::Capture::MediaCaptureFailedEventHandler;
using Windows::Media::Capture::MediaCaptureInitializationSettings;
using Windows::Media::Capture::MediaStreamType;
using Windows::Media::IMediaExtension;
using Windows::Media::MediaProperties::IVideoEncodingProperties;
using Windows::Media::MediaProperties::MediaEncodingProfile;
using Windows::Media::MediaProperties::MediaEncodingSubtypes;
using Windows::Media::MediaProperties::VideoEncodingProperties;
using Windows::UI::Core::DispatchedHandler;
using Windows::UI::Core::CoreDispatcherPriority;
using Windows::Foundation::IAsyncAction;
using Windows::Foundation::TypedEventHandler;

using Windows::System::Threading::TimerElapsedHandler;
using Windows::System::Threading::ThreadPoolTimer;


namespace webrtc {
namespace videocapturemodule {

void RunOnCoreDispatcher(std::function<void()> fn, bool async) {
  Windows::UI::Core::CoreDispatcher^ _windowDispatcher =
    VideoCommonWinUWP::GetCoreDispatcher();
  if (_windowDispatcher != nullptr) {
    auto handler = ref new Windows::UI::Core::DispatchedHandler([fn]() {
      fn();
    });
    auto action = _windowDispatcher->RunAsync(
      CoreDispatcherPriority::Normal, handler);
    if (async) {
      Concurrency::create_task(action);
    } else {
      Concurrency::create_task(action).wait();
    }
  } else {
    fn();
  }
}

AppStateDispatcher* AppStateDispatcher::instance_ = NULL;

AppStateDispatcher* AppStateDispatcher::Instance() {
  if (!instance_)
    instance_ = new AppStateDispatcher;
  return instance_;
}

AppStateDispatcher::AppStateDispatcher() :
  display_orientation_(DisplayOrientations::Portrait) {
}

void AppStateDispatcher::DisplayOrientationChanged(
  DisplayOrientations display_orientation) {
  display_orientation_ = display_orientation;
  for (auto obs_it = observers.begin(); obs_it != observers.end(); ++obs_it) {
    (*obs_it)->DisplayOrientationChanged(display_orientation);
  }
}

DisplayOrientations AppStateDispatcher::GetOrientation() const {
  return display_orientation_;
}

void AppStateDispatcher::AddObserver(AppStateObserver* observer) {
  observers.push_back(observer);
}
void AppStateDispatcher::RemoveObserver(AppStateObserver* observer) {
  for (auto obs_it = observers.begin(); obs_it != observers.end(); ++obs_it) {
    if (*obs_it == observer) {
      observers.erase(obs_it);
      break;
    }
  }
}

ref class DisplayOrientation sealed {
public:
  virtual ~DisplayOrientation();

internal:
  DisplayOrientation(DisplayOrientationListener* listener);
  void OnOrientationChanged(
    Windows::Graphics::Display::DisplayInformation^ sender,
    Platform::Object^ args);

  property DisplayOrientations orientation;
private:
  DisplayOrientationListener* listener_;
  DisplayInformation^ display_info;
  Windows::Foundation::EventRegistrationToken
    orientation_changed_registration_token_;
};

DisplayOrientation::~DisplayOrientation() {
  auto tmpDisplayInfo = display_info;
  auto tmpToken = orientation_changed_registration_token_;
  if (tmpDisplayInfo != nullptr) {
    RunOnCoreDispatcher([tmpDisplayInfo, tmpToken]() {
      tmpDisplayInfo->OrientationChanged::remove(tmpToken);
    }, true);  // Run async because it can deadlock with core thread.
  }
}

DisplayOrientation::DisplayOrientation(DisplayOrientationListener* listener)
  : listener_(listener) {
  RunOnCoreDispatcher([this]() {
    // GetForCurrentView() only works on a thread associated with
    // a CoreWindow.  If this doesn't work because we're running in
    // a background task then the orientation needs to come from the
    // foreground as a notification.
    try {
      display_info = DisplayInformation::GetForCurrentView();
      orientation = display_info->CurrentOrientation;
      orientation_changed_registration_token_ =
        display_info->OrientationChanged::add(
        ref new TypedEventHandler<DisplayInformation^,
        Platform::Object^>(this, &DisplayOrientation::OnOrientationChanged));
    }
    catch (...) {
      display_info = nullptr;
      orientation = Windows::Graphics::Display::DisplayOrientations::Portrait;
      LOG(LS_ERROR) << "DisplayOrientation could not be initialized.";
    }
  });
}

void DisplayOrientation::OnOrientationChanged(DisplayInformation^ sender,
  Platform::Object^ args) {
  orientation = sender->CurrentOrientation;
  if (listener_)
    listener_->OnDisplayOrientationChanged(sender->CurrentOrientation);
}

ref class CaptureDevice sealed {
 public:
  virtual ~CaptureDevice();

 internal:
  CaptureDevice(CaptureDeviceListener* capture_device_listener);

  void Initialize(Platform::String^ device_id);

  void CleanupSink();

  void CleanupMediaCapture();

  void Cleanup();

  void StartCapture(MediaEncodingProfile^ media_encoding_profile,
                    IVideoEncodingProperties^ video_encoding_properties);

  void StopCapture();

  bool CaptureStarted() { return capture_started_; }

  VideoCaptureCapability GetFrameInfo() { return frame_info_; }

  void OnCaptureFailed(MediaCapture^ sender,
                       MediaCaptureFailedEventArgs^ error_event_args);

  void OnMediaSample(Object^ sender, MediaSampleEventArgs^ args);

  property Platform::Agile<Windows::Media::Capture::MediaCapture> MediaCapture {
    Platform::Agile<Windows::Media::Capture::MediaCapture> get();
  }

 private:
  void RemovePaddingPixels(uint8_t* video_frame, size_t& video_frame_length);

 private:
  Platform::Agile<Windows::Media::Capture::MediaCapture> media_capture_;
  Platform::String^ device_id_;
  VideoCaptureMediaSinkProxyWinUWP^ media_sink_;
  Windows::Foundation::EventRegistrationToken
    media_capture_failed_event_registration_token_;
  Windows::Foundation::EventRegistrationToken
    media_sink_video_sample_event_registration_token_;

  CaptureDeviceListener* capture_device_listener_;

  bool capture_started_;
  VideoCaptureCapability frame_info_;
  std::unique_ptr<webrtc::EventWrapper> _stopped;
};

CaptureDevice::CaptureDevice(
  CaptureDeviceListener* capture_device_listener)
  : media_capture_(nullptr),
    device_id_(nullptr),
    media_sink_(nullptr),
    capture_device_listener_(capture_device_listener),
    capture_started_(false) {
  _stopped.reset(webrtc::EventWrapper::Create());
  _stopped->Set();
}

CaptureDevice::~CaptureDevice() {
}

void CaptureDevice::Initialize(Platform::String^ device_id) {
  LOG(LS_INFO) << "CaptureDevice::Initialize";
  device_id_ = device_id;
}

void CaptureDevice::CleanupSink() {
  if (media_sink_) {
    media_sink_->MediaSampleEvent -=
      media_sink_video_sample_event_registration_token_;
    delete media_sink_;
    media_sink_ = nullptr;
    capture_started_ = false;
  }
}

void CaptureDevice::CleanupMediaCapture() {
  Windows::Media::Capture::MediaCapture^ media_capture = media_capture_.Get();
  if (media_capture != nullptr) {
    media_capture->Failed -= media_capture_failed_event_registration_token_;
    MediaCaptureDevicesWinUWP::Instance()->RemoveMediaCapture(device_id_);
    media_capture_ = nullptr;
  }
}

Platform::Agile<Windows::Media::Capture::MediaCapture>
CaptureDevice::MediaCapture::get() {
  return media_capture_;
}

void CaptureDevice::Cleanup() {
  Windows::Media::Capture::MediaCapture^ media_capture = media_capture_.Get();
  if (media_capture == nullptr) {
    return;
  }
  if (capture_started_) {
    if (_stopped->Wait(5000) == kEventTimeout) {
      Concurrency::create_task(
        media_capture->StopRecordAsync()).
        then([this](Concurrency::task<void>& async_info) {
        try {
          async_info.get();
          CleanupSink();
          CleanupMediaCapture();
          device_id_ = nullptr;
          _stopped->Set();
        } catch (Platform::Exception^ e) {
          CleanupSink();
          CleanupMediaCapture();
          device_id_ = nullptr;
          _stopped->Set();
          throw;
        }
      }).wait();
    }
  } else {
    CleanupSink();
    CleanupMediaCapture();
    device_id_ = nullptr;
  }
}

void CaptureDevice::StartCapture(
  MediaEncodingProfile^ media_encoding_profile,
  IVideoEncodingProperties^ video_encoding_properties) {
  if (capture_started_) {
    throw ref new Platform::Exception(
      __HRESULT_FROM_WIN32(ERROR_INVALID_STATE));
  }

  if (_stopped->Wait(5000) == kEventTimeout) {
    throw ref new Platform::Exception(
      __HRESULT_FROM_WIN32(ERROR_INVALID_STATE));
  }

  CleanupSink();
  CleanupMediaCapture();

  if (device_id_ == nullptr) {
    LOG(LS_WARNING) << "Capture device is not initialized.";
    return;
  }

  frame_info_.width = media_encoding_profile->Video->Width;
  frame_info_.height = media_encoding_profile->Video->Height;
  frame_info_.maxFPS =
    static_cast<int>(
      static_cast<float>(
        media_encoding_profile->Video->FrameRate->Numerator) /
      static_cast<float>(
        media_encoding_profile->Video->FrameRate->Denominator));
  if (_wcsicmp(media_encoding_profile->Video->Subtype->Data(),
    MediaEncodingSubtypes::Yv12->Data()) == 0) {
    frame_info_.rawType = kVideoYV12;
  } else if (_wcsicmp(media_encoding_profile->Video->Subtype->Data(),
    MediaEncodingSubtypes::Yuy2->Data()) == 0) {
    frame_info_.rawType = kVideoYUY2;
  } else if (_wcsicmp(media_encoding_profile->Video->Subtype->Data(),
    MediaEncodingSubtypes::Iyuv->Data()) == 0) {
    frame_info_.rawType = kVideoIYUV;
  } else if (_wcsicmp(media_encoding_profile->Video->Subtype->Data(),
    MediaEncodingSubtypes::Rgb24->Data()) == 0) {
    frame_info_.rawType = kVideoRGB24;
  } else if (_wcsicmp(media_encoding_profile->Video->Subtype->Data(),
    MediaEncodingSubtypes::Rgb32->Data()) == 0) {
    frame_info_.rawType = kVideoARGB;
  } else if (_wcsicmp(media_encoding_profile->Video->Subtype->Data(),
    MediaEncodingSubtypes::Mjpg->Data()) == 0) {
    frame_info_.rawType = kVideoMJPEG;
  } else if (_wcsicmp(media_encoding_profile->Video->Subtype->Data(),
    MediaEncodingSubtypes::Nv12->Data()) == 0) {
    frame_info_.rawType = kVideoNV12;
  } else {
    frame_info_.rawType = kVideoUnknown;
  }

  media_capture_ =
    MediaCaptureDevicesWinUWP::Instance()->GetMediaCapture(device_id_);
  media_capture_failed_event_registration_token_ =
    media_capture_->Failed +=
      ref new MediaCaptureFailedEventHandler(this,
        &CaptureDevice::OnCaptureFailed);

#ifdef WIN10
  // Tell the video device controller to optimize for Low Latency then Power consumption
  media_capture_->VideoDeviceController->DesiredOptimization =
    Windows::Media::Devices::MediaCaptureOptimization::LatencyThenPower;
#endif

  media_sink_ = ref new VideoCaptureMediaSinkProxyWinUWP();
  media_sink_video_sample_event_registration_token_ =
    media_sink_->MediaSampleEvent +=
      ref new Windows::Foundation::EventHandler<MediaSampleEventArgs^>
        (this, &CaptureDevice::OnMediaSample);

  auto initOp = media_sink_->InitializeAsync(media_encoding_profile->Video);
  auto initTask = Concurrency::create_task(initOp)
    .then([this, media_encoding_profile,
      video_encoding_properties](IMediaExtension^ media_extension) {
      auto setPropOp =
        media_capture_->VideoDeviceController->SetMediaStreamPropertiesAsync(
        MediaStreamType::VideoRecord, video_encoding_properties);
      return Concurrency::create_task(setPropOp)
        .then([this, media_encoding_profile, media_extension]() {
          auto startRecordOp = media_capture_->StartRecordToCustomSinkAsync(
            media_encoding_profile, media_extension);
          return Concurrency::create_task(startRecordOp);
        });
      });

  initTask.then([this](Concurrency::task<void> async_info) {
    try {
      async_info.get();
      capture_started_ = true;
    } catch (Platform::Exception^ e) {
      LOG(LS_ERROR) << "StartRecordToCustomSinkAsync exception: "
                    << rtc::ToUtf8(e->Message->Data());
      CleanupSink();
      CleanupMediaCapture();
    }
  }).wait();

  LOG(LS_INFO) << "CaptureDevice::StartCapture: returning";
}

void CaptureDevice::StopCapture() {
  if (!capture_started_) {
    LOG(LS_INFO) << "CaptureDevice::StopCapture: called when never started";
    return;
  }
  Concurrency::create_task(
    media_capture_.Get()->StopRecordAsync()).
      then([this](Concurrency::task<void>& async_info) {
    try {
      async_info.get();
      CleanupSink();
      CleanupMediaCapture();
      _stopped->Set();
    } catch (Platform::Exception^ e) {
      CleanupSink();
      CleanupMediaCapture();
      _stopped->Set();
        LOG(LS_ERROR) <<
          "CaptureDevice::StopCapture: Stop failed, reason: '" <<
          rtc::ToUtf8(e->Message->Data()) << "'";
    }
  });
}

void CaptureDevice::OnCaptureFailed(
  Windows::Media::Capture::MediaCapture^ sender,
  MediaCaptureFailedEventArgs^ error_event_args) {
  if (capture_device_listener_) {
    capture_device_listener_->OnCaptureDeviceFailed(
      error_event_args->Code,
      error_event_args->Message);
  }
}

void CaptureDevice::OnMediaSample(Object^ sender, MediaSampleEventArgs^ args) {
  if (capture_device_listener_) {
    Microsoft::WRL::ComPtr<IMFSample> spMediaSample = args->GetMediaSample();
    ComPtr<IMFMediaBuffer> spMediaBuffer;
    HRESULT hr = spMediaSample->GetBufferByIndex(0, &spMediaBuffer);
    LONGLONG hnsSampleTime = 0;
    BYTE* pbBuffer = NULL;
    DWORD cbMaxLength = 0;
    DWORD cbCurrentLength = 0;

    if (SUCCEEDED(hr)) {
      hr = spMediaSample->GetSampleTime(&hnsSampleTime);
    }
    if (SUCCEEDED(hr)) {
      hr = spMediaBuffer->Lock(&pbBuffer, &cbMaxLength, &cbCurrentLength);
    }
    if (SUCCEEDED(hr)) {
      uint8_t* video_frame;
      size_t video_frame_length;
      int64_t capture_time;
      video_frame = pbBuffer;
      video_frame_length = cbCurrentLength;
      // conversion from 100-nanosecond to millisecond units
      capture_time = hnsSampleTime / 10000;

      RemovePaddingPixels(video_frame, video_frame_length);

      LOG(LS_VERBOSE) <<
        "Video Capture - Media sample received - video frame length: " <<
        video_frame_length << ", capture time : " << capture_time;

      capture_device_listener_->OnIncomingFrame(video_frame,
                                                video_frame_length,
                                                frame_info_);

      hr = spMediaBuffer->Unlock();
    }
    if (FAILED(hr)) {
      LOG(LS_ERROR) << "Failed to send media sample. " << hr;
    }
  }
}

void CaptureDevice::RemovePaddingPixels(uint8_t* video_frame,
                                        size_t& video_frame_length) {

  int padded_row_num = 16 - frame_info_.height % 16;
  int padded_col_num = 16 - frame_info_.width % 16;
  if (padded_row_num == 16)
    padded_row_num = 0;
  if (padded_col_num == 16)
    padded_col_num = 0;

  if (frame_info_.rawType == kVideoYV12 &&
    (int32_t)video_frame_length >
    frame_info_.width * frame_info_.height * 3 / 2) {
    uint8_t* src_video_y = video_frame;
    uint8_t* src_video_v = src_video_y +
      (frame_info_.width + padded_col_num) *
      (frame_info_.height + padded_row_num);
    uint8_t* src_video_u = src_video_v +
      (frame_info_.width + padded_col_num) *
      (frame_info_.height + padded_row_num) / 4;
    uint8_t* dst_video_y = src_video_y;
    uint8_t* dst_video_v = dst_video_y +
      frame_info_.width * frame_info_.height;
    uint8_t* dst_video_u = dst_video_v +
      frame_info_.width * frame_info_.height / 4;
    video_frame_length = frame_info_.width * frame_info_.height * 3 / 2;
    libyuv::CopyPlane(src_video_y, frame_info_.width + padded_col_num,
      dst_video_y, frame_info_.width,
      frame_info_.width, frame_info_.height);
    libyuv::CopyPlane(src_video_v, (frame_info_.width + padded_col_num) / 2,
      dst_video_v, frame_info_.width / 2,
      frame_info_.width / 2, frame_info_.height / 2);
    libyuv::CopyPlane(src_video_u, (frame_info_.width + padded_col_num) / 2,
      dst_video_u, frame_info_.width / 2,
      frame_info_.width / 2, frame_info_.height / 2);
  } else if (frame_info_.rawType == kVideoYUY2 &&
    (int32_t)video_frame_length >
    frame_info_.width * frame_info_.height * 2) {
    uint8_t* src_video = video_frame;
    uint8_t* dst_video = src_video;
    video_frame_length = frame_info_.width * frame_info_.height * 2;
    libyuv::CopyPlane(src_video, 2 * (frame_info_.width + padded_col_num),
      dst_video, 2 * frame_info_.width,
      2 * frame_info_.width, frame_info_.height);
  } else if (frame_info_.rawType == kVideoIYUV &&
    (int32_t)video_frame_length >
    frame_info_.width * frame_info_.height * 3 / 2) {
    uint8_t* src_video_y = video_frame;
    uint8_t* src_video_u = src_video_y +
      (frame_info_.width + padded_col_num) *
      (frame_info_.height + padded_row_num);
    uint8_t* src_video_v = src_video_u +
      (frame_info_.width + padded_col_num) *
      (frame_info_.height + padded_row_num) / 4;
    uint8_t* dst_video_y = src_video_y;
    uint8_t* dst_video_u = dst_video_y +
      frame_info_.width * frame_info_.height;
    uint8_t* dst_video_v = dst_video_u +
      frame_info_.width * frame_info_.height / 4;
    video_frame_length = frame_info_.width * frame_info_.height * 3 / 2;
    libyuv::CopyPlane(src_video_y, frame_info_.width + padded_col_num,
      dst_video_y, frame_info_.width,
      frame_info_.width, frame_info_.height);
    libyuv::CopyPlane(src_video_u, (frame_info_.width + padded_col_num) / 2,
      dst_video_u, frame_info_.width / 2,
      frame_info_.width / 2, frame_info_.height / 2);
    libyuv::CopyPlane(src_video_v, (frame_info_.width + padded_col_num) / 2,
      dst_video_v, frame_info_.width / 2,
      frame_info_.width / 2, frame_info_.height / 2);
  } else if (frame_info_.rawType == kVideoRGB24 &&
    (int32_t)video_frame_length >
    frame_info_.width * frame_info_.height * 3) {
    uint8_t* src_video = video_frame;
    uint8_t* dst_video = src_video;
    video_frame_length = frame_info_.width * frame_info_.height * 3;
    libyuv::CopyPlane(src_video, 3 * (frame_info_.width + padded_col_num),
      dst_video, 3 * frame_info_.width,
      3 * frame_info_.width, frame_info_.height);
  } else if (frame_info_.rawType == kVideoARGB &&
    (int32_t)video_frame_length >
    frame_info_.width * frame_info_.height * 4) {
    uint8_t* src_video = video_frame;
    uint8_t* dst_video = src_video;
    video_frame_length = frame_info_.width * frame_info_.height * 4;
    libyuv::CopyPlane(src_video, 4 * (frame_info_.width + padded_col_num),
      dst_video, 4 * frame_info_.width,
      4 * frame_info_.width, frame_info_.height);
  } else if (frame_info_.rawType == kVideoNV12 &&
    (int32_t)video_frame_length >
    frame_info_.width * frame_info_.height * 3 / 2) {
    uint8_t* src_video_y = video_frame;
    uint8_t* src_video_uv = src_video_y +
      (frame_info_.width + padded_col_num) *
      (frame_info_.height + padded_row_num);
    uint8_t* dst_video_y = src_video_y;
    uint8_t* dst_video_uv = dst_video_y +
      frame_info_.width * frame_info_.height;
    video_frame_length = frame_info_.width * frame_info_.height * 3 / 2;
    libyuv::CopyPlane(src_video_y, frame_info_.width + padded_col_num,
      dst_video_y, frame_info_.width,
      frame_info_.width, frame_info_.height);
    libyuv::CopyPlane(src_video_uv, frame_info_.width + padded_col_num,
      dst_video_uv, frame_info_.width,
      frame_info_.width, frame_info_.height / 2);
  }
}

ref class BlackFramesGenerator sealed {
public:
  virtual ~BlackFramesGenerator();

internal:
  BlackFramesGenerator(CaptureDeviceListener* capture_device_listener);

  void StartCapture(const VideoCaptureCapability& frame_info);

  void StopCapture();

  bool CaptureStarted() { return capture_started_; }

  void Cleanup();

  VideoCaptureCapability GetFrameInfo() { return frame_info_; }

private:
  CaptureDeviceListener* capture_device_listener_;
  bool capture_started_;
  VideoCaptureCapability frame_info_;
  ThreadPoolTimer^ timer_;
};

BlackFramesGenerator::BlackFramesGenerator(
  CaptureDeviceListener* capture_device_listener) :
  capture_started_(false), capture_device_listener_(capture_device_listener),
  timer_(nullptr) {
}

BlackFramesGenerator::~BlackFramesGenerator() {
  Cleanup();
}

void BlackFramesGenerator::StartCapture(
  const VideoCaptureCapability& frame_info) {
  frame_info_ = frame_info;
  frame_info_.rawType = kVideoRGB24;

  if (capture_started_) {
    LOG(LS_INFO) << "Black frame generator already started";
    throw ref new Platform::Exception(
      __HRESULT_FROM_WIN32(ERROR_INVALID_STATE));
  }
  LOG(LS_INFO) << "Starting black frame generator";

  size_t black_frame_size = frame_info_.width * frame_info_.height * 3;
  std::shared_ptr<std::vector<uint8_t>> black_frame(
    new std::vector<uint8_t>(black_frame_size, 0));
  auto handler = ref new TimerElapsedHandler(
    [this, black_frame_size, black_frame]
    (ThreadPoolTimer^ timer) -> void {
    if (this->capture_device_listener_ != nullptr){
      this->capture_device_listener_->OnIncomingFrame(
        black_frame->data(),
        black_frame_size,
        this->frame_info_);
    }
  });
  auto timespan = Windows::Foundation::TimeSpan();
  timespan.Duration = (1000 * 1000 * 10 /*1s in hns*/) /
                      frame_info_.maxFPS;
  timer_ = ThreadPoolTimer::CreatePeriodicTimer(handler, timespan);
  capture_started_ = true;
}

void BlackFramesGenerator::StopCapture() {
  if (!capture_started_) {
    throw ref new Platform::Exception(
      __HRESULT_FROM_WIN32(ERROR_INVALID_STATE));
  }

  LOG(LS_INFO) << "Stopping black frame generator";
  timer_->Cancel();
  timer_ = nullptr;
  capture_started_ = false;
}

void BlackFramesGenerator::Cleanup() {
  capture_device_listener_ = nullptr;
  if (capture_started_) {
    StopCapture();
  }
}

VideoCaptureWinUWP::VideoCaptureWinUWP()
  : device_(nullptr),
    camera_location_(Panel::Unknown),
    display_orientation_(nullptr),
    fake_device_(nullptr),
    last_frame_info_(),
    video_encoding_properties_(nullptr),
    media_encoding_profile_(nullptr) {
  if (VideoCommonWinUWP::GetCoreDispatcher() == nullptr) {
    LOG(LS_INFO) << "Using AppStateDispatcher as orientation source";
    AppStateDispatcher::Instance()->AddObserver(this);
  } else {
    // DisplayOrientation needs access to UI thread.
    LOG(LS_INFO) << "Using local detection for orientation source";
    display_orientation_ = ref new DisplayOrientation(this);
  }
}

VideoCaptureWinUWP::~VideoCaptureWinUWP() {
  if (device_ != nullptr)
    device_->Cleanup();
  if (fake_device_ != nullptr) {
    fake_device_->Cleanup();
  }
  if (display_orientation_ == nullptr) {
    AppStateDispatcher::Instance()->RemoveObserver(this);
  }
}

int32_t VideoCaptureWinUWP::Init(const char* device_unique_id) {
  CriticalSectionScoped cs(&_apiCs);
  const int32_t device_unique_id_length = (int32_t)strlen(device_unique_id);
  if (device_unique_id_length > kVideoCaptureUniqueNameLength) {
    LOG(LS_ERROR) << "Device name too long";
    return -1;
  }

  LOG(LS_INFO) << "Init called for device " << device_unique_id;

  device_id_ = nullptr;

  _deviceUniqueId = new (std::nothrow) char[device_unique_id_length + 1];
  memcpy(_deviceUniqueId, device_unique_id, device_unique_id_length + 1);

  Concurrency::create_task(
    DeviceInformation::FindAllAsync(DeviceClass::VideoCapture)).
    then([this, device_unique_id, device_unique_id_length](
      Concurrency::task<DeviceInformationCollection^> find_task) {
    try {
      DeviceInformationCollection^ dev_info_collection = find_task.get();
      if (dev_info_collection == nullptr || dev_info_collection->Size == 0) {
        LOG_F(LS_ERROR) << "No video capture device found";
        return;
      }
      // Look for the device in the collection.
      DeviceInformation^ chosen_dev_info = nullptr;
      for (unsigned int i = 0; i < dev_info_collection->Size; i++) {
        auto dev_info = dev_info_collection->GetAt(i);
        if (rtc::ToUtf8(dev_info->Id->Data()) == device_unique_id) {
          device_id_ = dev_info->Id;
          if (dev_info->EnclosureLocation != nullptr) {
            camera_location_ = dev_info->EnclosureLocation->Panel;
          } else {
            camera_location_ = Panel::Unknown;
          }
          break;
        }
      }
    }
    catch (Platform::Exception^ e) {
      LOG(LS_ERROR)
        << "Failed to retrieve device info collection. "
        << rtc::ToUtf8(e->Message->Data());
    }
  }).wait();

  if (device_id_ == nullptr) {
    LOG(LS_ERROR) << "No video capture device found";
    return -1;
  }

  device_ = ref new CaptureDevice(this);

  device_->Initialize(device_id_);

  fake_device_ = ref new BlackFramesGenerator(this);

  return 0;
}

int32_t VideoCaptureWinUWP::StartCapture(
  const VideoCaptureCapability& capability) {
  CriticalSectionScoped cs(&_apiCs);
  Platform::String^ subtype;
  switch (capability.rawType) {
  case kVideoYV12:
    subtype = MediaEncodingSubtypes::Yv12;
    break;
  case kVideoYUY2:
    subtype = MediaEncodingSubtypes::Yuy2;
    break;
  case kVideoI420:
  case kVideoIYUV:
    subtype = MediaEncodingSubtypes::Iyuv;
    break;
  case kVideoRGB24:
    subtype = MediaEncodingSubtypes::Rgb24;
    break;
  case kVideoARGB:
    subtype = MediaEncodingSubtypes::Argb32;
    break;
  case kVideoMJPEG:
    // MJPEG format is decoded internally by MF engine to NV12
    subtype = MediaEncodingSubtypes::Nv12;
    break;
  case kVideoNV12:
    subtype = MediaEncodingSubtypes::Nv12;
    break;
  default:
    LOG(LS_ERROR) <<
      "The specified raw video format is not supported on this plaform.";
    return -1;
  }

  media_encoding_profile_ = ref new MediaEncodingProfile();
  media_encoding_profile_->Audio = nullptr;
  media_encoding_profile_->Container = nullptr;
  media_encoding_profile_->Video = VideoEncodingProperties::CreateUncompressed(
    subtype, capability.width, capability.height);
  media_encoding_profile_->Video->FrameRate->Numerator = capability.maxFPS;
  media_encoding_profile_->Video->FrameRate->Denominator = 1;

  video_encoding_properties_ = nullptr;
  int min_width_diff = INT_MAX;
  int min_height_diff = INT_MAX;
  int min_fps_diff = INT_MAX;
  auto mediaCapture =
    MediaCaptureDevicesWinUWP::Instance()->GetMediaCapture(device_id_);
  auto streamProperties =
    mediaCapture->VideoDeviceController->GetAvailableMediaStreamProperties(
      MediaStreamType::VideoRecord);
  for (unsigned int i = 0; i < streamProperties->Size; i++) {
    IVideoEncodingProperties^ prop =
      static_cast<IVideoEncodingProperties^>(streamProperties->GetAt(i));

    if (capability.rawType != kVideoMJPEG &&
      _wcsicmp(prop->Subtype->Data(), subtype->Data()) != 0 ||
      capability.rawType == kVideoMJPEG &&
      _wcsicmp(prop->Subtype->Data(),
        MediaEncodingSubtypes::Mjpg->Data()) != 0) {
      continue;
    }

    int width_diff = abs(static_cast<int>(prop->Width - capability.width));
    int height_diff = abs(static_cast<int>(prop->Height - capability.height));
    int prop_fps = static_cast<int>(
      (static_cast<float>(prop->FrameRate->Numerator) /
      static_cast<float>(prop->FrameRate->Denominator)));
    int fps_diff = abs(static_cast<int>(prop_fps - capability.maxFPS));

    if (width_diff < min_width_diff) {
      video_encoding_properties_ = prop;
      min_width_diff = width_diff;
      min_height_diff = height_diff;
      min_fps_diff = fps_diff;
    } else if (width_diff == min_width_diff) {
      if (height_diff < min_height_diff) {
        video_encoding_properties_ = prop;
        min_height_diff = height_diff;
        min_fps_diff = fps_diff;
      } else if (height_diff == min_height_diff) {
        if (fps_diff < min_fps_diff) {
          video_encoding_properties_ = prop;
          min_fps_diff = fps_diff;
        }
      }
    }
  }
  try {
    if (display_orientation_) {
      ApplyDisplayOrientation(display_orientation_->orientation);
    } else {
      ApplyDisplayOrientation(AppStateDispatcher::Instance()->GetOrientation());
    }
    device_->StartCapture(media_encoding_profile_,
                          video_encoding_properties_);
    last_frame_info_ = capability;
  } catch (Platform::Exception^ e) {
    LOG(LS_ERROR) << "Failed to start capture. "
      << rtc::ToUtf8(e->Message->Data());
    return -1;
  }

  return 0;
}

void VideoCaptureWinUWP::ApplyDisplayOrientation(
  DisplayOrientations orientation) {
  if (camera_location_ == Windows::Devices::Enumeration::Panel::Unknown)
    return;
  switch (orientation) {
  case Windows::Graphics::Display::DisplayOrientations::Portrait:
    if (camera_location_ == Windows::Devices::Enumeration::Panel::Front)
      SetCaptureRotation(VideoRotation::kVideoRotation_270);
    else
      SetCaptureRotation(VideoRotation::kVideoRotation_90);
    break;
  case Windows::Graphics::Display::DisplayOrientations::PortraitFlipped:
    if (camera_location_ == Windows::Devices::Enumeration::Panel::Front)
      SetCaptureRotation(VideoRotation::kVideoRotation_90);
    else
      SetCaptureRotation(VideoRotation::kVideoRotation_270);
    break;
  case Windows::Graphics::Display::DisplayOrientations::Landscape:
    SetCaptureRotation(VideoRotation::kVideoRotation_0);
    break;
  case Windows::Graphics::Display::DisplayOrientations::LandscapeFlipped:
    SetCaptureRotation(VideoRotation::kVideoRotation_180);
    break;
  default:
    SetCaptureRotation(VideoRotation::kVideoRotation_0);
    break;
  }
}

int32_t VideoCaptureWinUWP::StopCapture() {
  CriticalSectionScoped cs(&_apiCs);

  try {
    if (device_->CaptureStarted()) {
      device_->StopCapture();
    }
    if (fake_device_->CaptureStarted()) {
      fake_device_->StopCapture();
    }
  } catch (Platform::Exception^ e) {
    LOG(LS_ERROR) << "Failed to stop capture. "
      << rtc::ToUtf8(e->Message->Data());
    return -1;
  }
  return 0;
}

bool VideoCaptureWinUWP::CaptureStarted() {
  CriticalSectionScoped cs(&_apiCs);

  return device_->CaptureStarted() || fake_device_->CaptureStarted();
}

int32_t VideoCaptureWinUWP::CaptureSettings(VideoCaptureCapability& settings) {
  CriticalSectionScoped cs(&_apiCs);
  settings = device_->GetFrameInfo();
  return 0;
}

bool VideoCaptureWinUWP::SuspendCapture() {
  if (device_->CaptureStarted()) {
    LOG(LS_INFO) << "SuspendCapture";
    device_->StopCapture();
    fake_device_->StartCapture(last_frame_info_);
    return true;
  }
  LOG(LS_INFO) << "SuspendCapture capture is not started";
  return false;
}

bool VideoCaptureWinUWP::ResumeCapture() {
  if (fake_device_->CaptureStarted()) {
    LOG(LS_INFO) << "ResumeCapture";
    fake_device_->StopCapture();
    device_->StartCapture(media_encoding_profile_,
      video_encoding_properties_);
    return true;
  }
  LOG(LS_INFO) << "ResumeCapture, capture is not started";
  return false;
}

bool VideoCaptureWinUWP::IsSuspended() {
  return fake_device_->CaptureStarted();
}

void VideoCaptureWinUWP::DisplayOrientationChanged(
  Windows::Graphics::Display::DisplayOrientations display_orientation) {
  if (display_orientation_ != nullptr) {
    LOG(LS_WARNING) <<
      "Ignoring orientation change notification from AppStateDispatcher";
    return;
  }
  ApplyDisplayOrientation(display_orientation);
}

void VideoCaptureWinUWP::OnDisplayOrientationChanged(
  DisplayOrientations orientation) {
  ApplyDisplayOrientation(orientation);
}

void VideoCaptureWinUWP::OnIncomingFrame(
  uint8_t* video_frame,
  size_t video_frame_length,
  const VideoCaptureCapability& frame_info) {
  if (device_->CaptureStarted()) {
    last_frame_info_ = frame_info;
  }
  IncomingFrame(video_frame, video_frame_length, frame_info);
}

void VideoCaptureWinUWP::OnCaptureDeviceFailed(HRESULT code,
                                              Platform::String^ message) {
  LOG(LS_ERROR) << "Capture device failed. HRESULT: " <<
    code << " Message: " << rtc::ToUtf8(message->Data());
  CriticalSectionScoped cs(&_apiCs);
  if (device_ != nullptr && device_->CaptureStarted()) {
    try {
      device_->StopCapture();
    } catch (Platform::Exception^ ex) {
      LOG(LS_WARNING) <<
        "Capture device failed: failed to stop ex='"
        << rtc::ToUtf8(ex->Message->Data()) << "'";
    }
  }
}

}  // namespace videocapturemodule
}  // namespace webrtc
