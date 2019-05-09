#ifndef HOLOLIGHT_REMOTING_SDP_OBSERVERS_H
#define HOLOLIGHT_REMOTING_SDP_OBSERVERS_H

#include "api/peerconnectioninterface.h"

namespace hololight::remoting {
class Connection;

class CreateSdpObserver : public webrtc::CreateSessionDescriptionObserver {
 public:
  CreateSdpObserver() = delete;
  CreateSdpObserver(rtc::WeakPtr<Connection> ptr) : connection_(ptr) {}
  void OnSuccess(webrtc::SessionDescriptionInterface* desc) override {
    RTC_LOG(LS_INFO) << "SDP OnSuccess called from "
                     << rtc::ThreadManager::Instance()->CurrentThread()->name();

    // set sdp on peerconnection
  }
  void OnFailure(webrtc::RTCError error) override {
    RTC_LOG(LS_ERROR) << error.message();
  }

 private:
  rtc::WeakPtr<Connection> connection_;
};

class SetSdpObserver : public webrtc::SetSessionDescriptionObserver {
 public:
  SetSdpObserver() = delete;
  SetSdpObserver(rtc::WeakPtr<Connection> ptr) : connection_(ptr) {}
  void OnSuccess() override {
    RTC_LOG(LS_INFO) << "SDP OnSuccess called from "
                     << rtc::ThreadManager::Instance()->CurrentThread()->name();
  }
  void OnFailure(webrtc::RTCError error) override {
      RTC_LOG(LS_ERROR) << error.message();
  }

 private:
  rtc::WeakPtr<Connection> connection_;
};
}  // namespace hololight::remoting
#endif  // HOLOLIGHT_REMOTING_SDP_OBSERVERS_H