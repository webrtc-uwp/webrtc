/*
 * libjingle
 * Copyright 2004 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TALK_MEDIA_DEVICES_WINUWPDEVICEMANAGER_H_
#define TALK_MEDIA_DEVICES_WINUWPDEVICEMANAGER_H_

#ifndef WINUWP
#error Invalid build configuration
#endif  // WINUWP

#include <map>
#include <string>
#include <vector>

#include "webrtc/media/base/device.h"
#include "webrtc/media/base/videocapturer.h"
#include "webrtc/media/base/videocapturerfactory.h"
#include "webrtc/base/sigslot.h"
#include "webrtc/base/stringencode.h"
#include "webrtc/base/logging.h"

namespace cricket {


class WinUWPDeviceManager {
 public:
  WinUWPDeviceManager();
  virtual ~WinUWPDeviceManager();

	sigslot::signal0<> SignalDevicesChange;
	static const char kDefaultDeviceName[];

  // Initialization
  bool Init();
  void Terminate();

  bool GetAudioInputDevices(std::vector<Device>* devices);
  bool GetAudioOutputDevices(std::vector<Device>* devices);

  bool GetVideoCaptureDevices(std::vector<Device>* devs);
	VideoCapturer* WinUWPDeviceManager::CreateVideoCapturer(const Device& device) const;

	virtual void SetVideoDeviceCapturerFactory(
		VideoDeviceCapturerFactory* video_device_capturer_factory) {
		video_device_capturer_factory_.reset(video_device_capturer_factory);
	}

 protected:
	 bool IsInWhitelist(const std::string& key, VideoFormat* video_format) const;
	 bool GetMaxFormat(const Device& device, VideoFormat* video_format) const;
 private:
  bool GetDefaultVideoCaptureDevice(Device* device);
  void OnDeviceChange();

  static const char* kUsbDevicePathPrefix;
	bool initialized_;
	std::unique_ptr<VideoDeviceCapturerFactory> video_device_capturer_factory_;
	std::map<std::string, VideoFormat> max_formats_;

  ref class WinUWPWatcher sealed {
   public:
    WinUWPWatcher();
    void Start();
    void Stop();

   private:
    friend WinUWPDeviceManager;
    WinUWPDeviceManager* deviceManager_;
    Windows::Devices::Enumeration::DeviceWatcher^ videoCaptureWatcher_;
    Windows::Devices::Enumeration::DeviceWatcher^ videoAudioInWatcher_;
    Windows::Devices::Enumeration::DeviceWatcher^ videoAudioOutWatcher_;

    void OnVideoCaptureAdded(Windows::Devices::Enumeration::DeviceWatcher
      ^sender, Windows::Devices::Enumeration::DeviceInformation ^args);
    void OnVideoCaptureRemoved(Windows::Devices::Enumeration::DeviceWatcher
      ^sender, Windows::Devices::Enumeration::DeviceInformationUpdate ^args);

    void OnAudioInAdded(Windows::Devices::Enumeration::DeviceWatcher
      ^sender, Windows::Devices::Enumeration::DeviceInformation ^args);
    void OnAudioInRemoved(Windows::Devices::Enumeration::DeviceWatcher ^sender,
      Windows::Devices::Enumeration::DeviceInformationUpdate ^args);

    void OnAudioOutAdded(Windows::Devices::Enumeration::DeviceWatcher ^sender,
      Windows::Devices::Enumeration::DeviceInformation ^args);
    void OnAudioOutRemoved(Windows::Devices::Enumeration::DeviceWatcher
      ^sender, Windows::Devices::Enumeration::DeviceInformationUpdate ^args);

    void OnDeviceChange();
  };

  WinUWPWatcher^ watcher_;
};
class DeviceManagerFactory {
	public:
		static WinUWPDeviceManager* Create();

	private:
		DeviceManagerFactory() {}
};
}  // namespace cricket

#endif  // TALK_MEDIA_DEVICES_WINUWPDEVICEMANAGER_H_
