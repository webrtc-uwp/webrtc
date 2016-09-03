
// Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef WINRT
#error Invalid build configuration
#endif  // WINRT
#include "webrtc/media/devices/winrtdevicemanager.h"
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
#include "webrtc/media/base/mediacommon.h"

namespace cricket {
  const char* WinRTDeviceManager::kUsbDevicePathPrefix = "\\\\?\\usb";

  DeviceManagerInterface* DeviceManagerFactory::Create() {
    return new WinRTDeviceManager();
  }

  WinRTDeviceManager::WinRTDeviceManager() :
    watcher_(ref new WinRTWatcher()) {}

  WinRTDeviceManager::~WinRTDeviceManager() {
    if (initialized()) {
      Terminate();
    }
  }

  bool WinRTDeviceManager::Init() {
    if (!initialized()) {
      watcher_->deviceManager_ = this;
      watcher_->Start();
      set_initialized(true);
    }
    return true;
  }

  void WinRTDeviceManager::Terminate() {
    if (initialized()) {
      watcher_->Stop();
      set_initialized(false);
    }
  }

  bool WinRTDeviceManager::GetAudioInputDevices(std::vector<Device>* devices) {
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

  bool WinRTDeviceManager::GetAudioOutputDevices(
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

  bool WinRTDeviceManager::GetVideoCaptureDevices(
    std::vector<Device>* devices) {
    devices->clear();
    rtc::scoped_ptr<webrtc::VideoCaptureModule::DeviceInfo> devInfo =
      rtc::scoped_ptr<webrtc::VideoCaptureModule::DeviceInfo>(
      webrtc::VideoCaptureFactory::CreateDeviceInfo(0));
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

  bool WinRTDeviceManager::GetDefaultVideoCaptureDevice(Device* device) {
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

  void WinRTDeviceManager::OnDeviceChange() {
    SignalDevicesChange();
  }

  WinRTDeviceManager::WinRTWatcher::WinRTWatcher() :
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
      this, &WinRTDeviceManager::WinRTWatcher::OnVideoCaptureAdded);
    videoCaptureWatcher_->Removed +=
      ref new Windows::Foundation::TypedEventHandler<
      Windows::Devices::Enumeration::DeviceWatcher ^,
      Windows::Devices::Enumeration::DeviceInformationUpdate ^>(this,
      &WinRTDeviceManager::WinRTWatcher::OnVideoCaptureRemoved);

    videoAudioInWatcher_->Added +=
      ref new Windows::Foundation::TypedEventHandler<
      Windows::Devices::Enumeration::DeviceWatcher ^,
      Windows::Devices::Enumeration::DeviceInformation ^>(
      this, &WinRTDeviceManager::WinRTWatcher::OnAudioInAdded);
    videoAudioInWatcher_->Removed +=
      ref new Windows::Foundation::TypedEventHandler<
      Windows::Devices::Enumeration::DeviceWatcher ^,
      Windows::Devices::Enumeration::DeviceInformationUpdate ^>(this,
      &WinRTDeviceManager::WinRTWatcher::OnAudioInRemoved);

    videoAudioOutWatcher_->Added +=
      ref new Windows::Foundation::TypedEventHandler<
      Windows::Devices::Enumeration::DeviceWatcher ^,
      Windows::Devices::Enumeration::DeviceInformation ^>(
      this, &WinRTDeviceManager::WinRTWatcher::OnAudioOutAdded);
    videoAudioOutWatcher_->Removed +=
      ref new Windows::Foundation::TypedEventHandler<
      Windows::Devices::Enumeration::DeviceWatcher ^,
      Windows::Devices::Enumeration::DeviceInformationUpdate ^>(this,
      &WinRTDeviceManager::WinRTWatcher::OnAudioOutRemoved);
  }

  void WinRTDeviceManager::WinRTWatcher::OnVideoCaptureAdded(
    Windows::Devices::Enumeration::DeviceWatcher ^sender,
    Windows::Devices::Enumeration::DeviceInformation ^args) {
    OnDeviceChange();
  }

  void WinRTDeviceManager::WinRTWatcher::OnVideoCaptureRemoved(
    Windows::Devices::Enumeration::DeviceWatcher ^sender,
    Windows::Devices::Enumeration::DeviceInformationUpdate ^args) {
    OnDeviceChange();
  }

  void WinRTDeviceManager::WinRTWatcher::OnAudioInAdded(
    Windows::Devices::Enumeration::DeviceWatcher ^sender,
    Windows::Devices::Enumeration::DeviceInformation ^args) {
    OnDeviceChange();
  }

  void WinRTDeviceManager::WinRTWatcher::OnAudioInRemoved(
    Windows::Devices::Enumeration::DeviceWatcher ^sender,
    Windows::Devices::Enumeration::DeviceInformationUpdate ^args) {
    OnDeviceChange();
  }

  void WinRTDeviceManager::WinRTWatcher::OnAudioOutAdded(
    Windows::Devices::Enumeration::DeviceWatcher ^sender,
    Windows::Devices::Enumeration::DeviceInformation ^args) {
    OnDeviceChange();
  }

  void WinRTDeviceManager::WinRTWatcher::OnAudioOutRemoved(
    Windows::Devices::Enumeration::DeviceWatcher ^sender,
    Windows::Devices::Enumeration::DeviceInformationUpdate ^args) {
    OnDeviceChange();
  }

  void WinRTDeviceManager::WinRTWatcher::Start() {
    videoCaptureWatcher_->Start();
    videoAudioInWatcher_->Start();
    videoAudioOutWatcher_->Start();
  }

  void WinRTDeviceManager::WinRTWatcher::Stop() {
    videoCaptureWatcher_->Stop();
    videoAudioInWatcher_->Stop();
    videoAudioOutWatcher_->Stop();
  }

  void WinRTDeviceManager::WinRTWatcher::OnDeviceChange() {
    deviceManager_->OnDeviceChange();
  }

}  // namespace cricket
