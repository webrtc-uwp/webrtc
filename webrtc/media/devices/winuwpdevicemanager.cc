
// Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef WINUWP
#error Invalid build configuration
#endif  // WINUWP
#include "webrtc/media/devices/winuwpdevicemanager.h"
#include "webrtc/media/engine/webrtcvideocapturerfactory.h"
#include <dbt.h>
#include <strmif.h>
#include <ks.h>
#include <ksmedia.h>
#include <mmdeviceapi.h>
#include <mmsystem.h>
#include <functiondiscoverykeys_devpkey.h>
#include <wincodec.h>
#include <uuids.h>
#include <ppltasks.h>
#include <collection.h>

#include "webrtc/base/logging.h"
#include "webrtc/base/stringutils.h"
#include "webrtc/base/thread.h"
#include "webrtc/base/win32.h"  // ToUtf8
#include "webrtc/base/win32window.h"
#include "webrtc/modules/video_capture/video_capture_factory.h"

namespace {

	bool StringMatchWithWildcard(
		const std::pair<const std::basic_string<char>, cricket::VideoFormat> key,
		const std::string& val) {
		return rtc::string_match(val.c_str(), key.first.c_str());
	}

}  // namespace

namespace cricket {
  const char* WinUWPDeviceManager::kUsbDevicePathPrefix = "\\\\?\\usb";
	// Initialize to empty string.
	const char WinUWPDeviceManager::kDefaultDeviceName[] = "";

	WinUWPDeviceManager* DeviceManagerFactory::Create() {
    return new WinUWPDeviceManager();
  }

	WinUWPDeviceManager::WinUWPDeviceManager() :
		watcher_(ref new WinUWPWatcher()), initialized_(false) {
#ifdef HAVE_WEBRTC_VIDEO
		SetVideoDeviceCapturerFactory(new WebRtcVideoDeviceCapturerFactory());
#endif  // HAVE_WEBRTC_VIDEO
	}

  WinUWPDeviceManager::~WinUWPDeviceManager() {
    if (initialized_) {
      Terminate();
    }
  }

  bool WinUWPDeviceManager::Init() {
    if (!initialized_) {
      watcher_->deviceManager_ = this;
      watcher_->Start();
			initialized_ = true;
    }
    return true;
  }

  void WinUWPDeviceManager::Terminate() {
    if (initialized_) {
      watcher_->Stop();
			initialized_ = false;
    }
  }

  bool WinUWPDeviceManager::GetAudioInputDevices(std::vector<Device>* devices) {
    devices->clear();
    auto deviceOp = Windows::Devices::Enumeration::
      DeviceInformation::FindAllAsync(
      Windows::Devices::Enumeration::DeviceClass::AudioCapture);
    auto deviceEnumTask = concurrency::create_task(deviceOp);
    deviceEnumTask.then([&devices](
      Windows::Devices::Enumeration::DeviceInformationCollection^
          deviceCollection) {
      for (size_t i = 0; i < deviceCollection->Size; i++) {
        Windows::Devices::Enumeration::DeviceInformation^ di =
                                                deviceCollection->GetAt(i);
        std::string nameUTF8(rtc::ToUtf8(di->Name->Data(),
          di->Name->Length()));
        std::string idUTF8(rtc::ToUtf8(di->Id->Data(), di->Id->Length()));
        devices->push_back(Device(nameUTF8, idUTF8));
      }
    }).wait();
    return true;
  }

  bool WinUWPDeviceManager::GetAudioOutputDevices(
    std::vector<Device>* devices) {
    devices->clear();
    auto deviceOp =
      Windows::Devices::Enumeration::DeviceInformation::FindAllAsync(
      Windows::Devices::Enumeration::DeviceClass::AudioRender);
    auto deviceEnumTask = concurrency::create_task(deviceOp);
    deviceEnumTask.then([&devices](
      Windows::Devices::Enumeration::DeviceInformationCollection^
        deviceCollection) {
      for (size_t i = 0; i < deviceCollection->Size; i++) {
        Windows::Devices::Enumeration::DeviceInformation^ di =
            deviceCollection->GetAt(i);
        std::string nameUTF8(rtc::ToUtf8(di->Name->Data(),
          di->Name->Length()));
        std::string idUTF8(rtc::ToUtf8(di->Id->Data(), di->Id->Length()));
        devices->push_back(Device(nameUTF8, idUTF8));
      }
    }).wait();
    return true;
  }

  bool WinUWPDeviceManager::GetVideoCaptureDevices(
    std::vector<Device>* devices) {
    devices->clear();
    std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo> devInfo =
      std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo>(
      webrtc::VideoCaptureFactory::CreateDeviceInfo());
    int deviceCount = devInfo->NumberOfDevices();
    const unsigned int KMaxDeviceNameLength = 128;
    const unsigned int KMaxUniqueIdLength = 256;
    char deviceName[KMaxDeviceNameLength];
    char uniqueId[KMaxUniqueIdLength];
    for (int i = 0; i < deviceCount; i++) {
      devInfo->GetDeviceName(i, deviceName,
        KMaxDeviceNameLength, uniqueId,
        KMaxUniqueIdLength);
      devices->push_back(Device(std::string(deviceName),
        std::string(uniqueId)));
    }
    return true;
  }

  bool WinUWPDeviceManager::GetDefaultVideoCaptureDevice(Device* device) {
    std::vector<Device> devices;
    bool ret = (GetVideoCaptureDevices(&devices)) && (!devices.empty());
    if (ret) {
      *device = *devices.begin();
      for (std::vector<Device>::const_iterator it = devices.begin();
        it != devices.end(); it++) {
        if (strnicmp((*it).id.c_str(), kUsbDevicePathPrefix,
          sizeof(kUsbDevicePathPrefix) - 1) == 0) {
          *device = *it;
          break;
        }
      }
    }
    return ret;
  }

	VideoCapturer* WinUWPDeviceManager::CreateVideoCapturer(const Device& device) const {
		if (!video_device_capturer_factory_) {
			LOG(LS_ERROR) << "No video capturer factory for devices.";
			return NULL;
		}
		std::unique_ptr<cricket::VideoCapturer> capturer =
			video_device_capturer_factory_->Create(device);
		if (!capturer) {
			return NULL;
		}
		LOG(LS_INFO) << "Created VideoCapturer for " << device.name;
		VideoFormat video_format;
		bool has_max = GetMaxFormat(device, &video_format);
		capturer->set_enable_camera_list(has_max);
		if (has_max) {
			capturer->ConstrainSupportedFormats(video_format);
		}
		return capturer.release();
	}

	bool WinUWPDeviceManager::GetMaxFormat(const Device& device,
		VideoFormat* video_format) const {
		// Match USB ID if available. Failing that, match device name.
#if !defined(WINUWP)
		std::string usb_id;
		if (GetUsbId(device, &usb_id) && IsInWhitelist(usb_id, video_format)) {
			return true;
		}
#endif
		return IsInWhitelist(device.name, video_format);
	}

	bool WinUWPDeviceManager::IsInWhitelist(const std::string& key,
		VideoFormat* video_format) const {
		std::map<std::string, VideoFormat>::const_iterator found =
			std::search_n(max_formats_.begin(), max_formats_.end(), 1, key,
				StringMatchWithWildcard);
		if (found == max_formats_.end()) {
			return false;
		}
		*video_format = found->second;
		return true;
	}
  void WinUWPDeviceManager::OnDeviceChange() {
    SignalDevicesChange();
  }

  WinUWPDeviceManager::WinUWPWatcher::WinUWPWatcher() :
    deviceManager_(nullptr),
    videoCaptureWatcher_(
      Windows::Devices::Enumeration::DeviceInformation::CreateWatcher(
    Windows::Devices::Enumeration::DeviceClass::VideoCapture)),
    videoAudioInWatcher_(
      Windows::Devices::Enumeration::DeviceInformation::CreateWatcher(
    Windows::Devices::Enumeration::DeviceClass::AudioCapture)),
    videoAudioOutWatcher_(
      Windows::Devices::Enumeration::DeviceInformation::CreateWatcher(
    Windows::Devices::Enumeration::DeviceClass::AudioRender)) {
    videoCaptureWatcher_->Added +=
      ref new Windows::Foundation::TypedEventHandler<
      Windows::Devices::Enumeration::DeviceWatcher ^,
      Windows::Devices::Enumeration::DeviceInformation ^>(
      this, &WinUWPDeviceManager::WinUWPWatcher::OnVideoCaptureAdded);
    videoCaptureWatcher_->Removed +=
      ref new Windows::Foundation::TypedEventHandler<
      Windows::Devices::Enumeration::DeviceWatcher ^,
      Windows::Devices::Enumeration::DeviceInformationUpdate ^>(this,
      &WinUWPDeviceManager::WinUWPWatcher::OnVideoCaptureRemoved);

    videoAudioInWatcher_->Added +=
      ref new Windows::Foundation::TypedEventHandler<
      Windows::Devices::Enumeration::DeviceWatcher ^,
      Windows::Devices::Enumeration::DeviceInformation ^>(
      this, &WinUWPDeviceManager::WinUWPWatcher::OnAudioInAdded);
    videoAudioInWatcher_->Removed +=
      ref new Windows::Foundation::TypedEventHandler<
      Windows::Devices::Enumeration::DeviceWatcher ^,
      Windows::Devices::Enumeration::DeviceInformationUpdate ^>(this,
      &WinUWPDeviceManager::WinUWPWatcher::OnAudioInRemoved);

    videoAudioOutWatcher_->Added +=
      ref new Windows::Foundation::TypedEventHandler<
      Windows::Devices::Enumeration::DeviceWatcher ^,
      Windows::Devices::Enumeration::DeviceInformation ^>(
      this, &WinUWPDeviceManager::WinUWPWatcher::OnAudioOutAdded);
    videoAudioOutWatcher_->Removed +=
      ref new Windows::Foundation::TypedEventHandler<
      Windows::Devices::Enumeration::DeviceWatcher ^,
      Windows::Devices::Enumeration::DeviceInformationUpdate ^>(this,
      &WinUWPDeviceManager::WinUWPWatcher::OnAudioOutRemoved);
  }

  void WinUWPDeviceManager::WinUWPWatcher::OnVideoCaptureAdded(
    Windows::Devices::Enumeration::DeviceWatcher ^sender,
    Windows::Devices::Enumeration::DeviceInformation ^args) {
    OnDeviceChange();
  }

  void WinUWPDeviceManager::WinUWPWatcher::OnVideoCaptureRemoved(
    Windows::Devices::Enumeration::DeviceWatcher ^sender,
    Windows::Devices::Enumeration::DeviceInformationUpdate ^args) {
    OnDeviceChange();
  }

  void WinUWPDeviceManager::WinUWPWatcher::OnAudioInAdded(
    Windows::Devices::Enumeration::DeviceWatcher ^sender,
    Windows::Devices::Enumeration::DeviceInformation ^args) {
    OnDeviceChange();
  }

  void WinUWPDeviceManager::WinUWPWatcher::OnAudioInRemoved(
    Windows::Devices::Enumeration::DeviceWatcher ^sender,
    Windows::Devices::Enumeration::DeviceInformationUpdate ^args) {
    OnDeviceChange();
  }

  void WinUWPDeviceManager::WinUWPWatcher::OnAudioOutAdded(
    Windows::Devices::Enumeration::DeviceWatcher ^sender,
    Windows::Devices::Enumeration::DeviceInformation ^args) {
    OnDeviceChange();
  }

  void WinUWPDeviceManager::WinUWPWatcher::OnAudioOutRemoved(
    Windows::Devices::Enumeration::DeviceWatcher ^sender,
    Windows::Devices::Enumeration::DeviceInformationUpdate ^args) {
    OnDeviceChange();
  }

  void WinUWPDeviceManager::WinUWPWatcher::Start() {
    videoCaptureWatcher_->Start();
    videoAudioInWatcher_->Start();
    videoAudioOutWatcher_->Start();
  }

  void WinUWPDeviceManager::WinUWPWatcher::Stop() {
    videoCaptureWatcher_->Stop();
    videoAudioInWatcher_->Stop();
    videoAudioOutWatcher_->Stop();
  }

  void WinUWPDeviceManager::WinUWPWatcher::OnDeviceChange() {
    deviceManager_->OnDeviceChange();
  }

}  // namespace cricket
