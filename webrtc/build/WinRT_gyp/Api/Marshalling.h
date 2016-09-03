
// Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef WEBRTC_BUILD_WINRT_GYP_API_MARSHALLING_H_
#define WEBRTC_BUILD_WINRT_GYP_API_MARSHALLING_H_

#include <string>
#include <vector>
#include "webrtc/api/peerconnectioninterface.h"
#include "Media.h"
#include "webrtc/api/jsep.h"
#include "PeerConnectionInterface.h"

using Platform::String;
using Windows::Foundation::Collections::IVector;

#define DECLARE_MARSHALLED_ENUM(winrtType, nativeType) \
  void FromCx(winrtType inObj, nativeType* outObj);\
  void ToCx(nativeType const& inObj, winrtType* outObj)

// Marshalling functions to convert from WinRT objects to native cpp.
namespace webrtc_winrt_api_internal {

  std::string FromCx(String^ inObj);
  String^ ToCx(std::string const& inObj);

  DECLARE_MARSHALLED_ENUM(webrtc_winrt_api::RTCBundlePolicy,
                          webrtc::PeerConnectionInterface::BundlePolicy);
  DECLARE_MARSHALLED_ENUM(webrtc_winrt_api::RTCIceTransportPolicy,
                          webrtc::PeerConnectionInterface::IceTransportsType);
  DECLARE_MARSHALLED_ENUM(webrtc_winrt_api::RTCSignalingState,
                          webrtc::PeerConnectionInterface::SignalingState);
  DECLARE_MARSHALLED_ENUM(webrtc_winrt_api::RTCDataChannelState,
                          webrtc::DataChannelInterface::DataState);
  DECLARE_MARSHALLED_ENUM(webrtc_winrt_api::RTCIceGatheringState,
                          webrtc::PeerConnectionInterface::IceGatheringState);
  DECLARE_MARSHALLED_ENUM(webrtc_winrt_api::RTCIceConnectionState,
                          webrtc::PeerConnectionInterface::IceConnectionState);
  DECLARE_MARSHALLED_ENUM(webrtc_winrt_api::RTCStatsType,
                          webrtc::StatsReport::StatsType);
  DECLARE_MARSHALLED_ENUM(webrtc_winrt_api::RTCStatsValueName,
                          webrtc::StatsReport::StatsValueName);

  template <typename I, typename O>
  void FromCx(
    IVector<I>^ inArray,
    std::vector<O>* outArray) {
	  
	outArray->resize(inArray->Size);
	int index = 0;
    for (I inObj : inArray) {
      FromCx(inObj, &(*outArray)[index]);
      index++;
    }
  }

  template <typename I, typename O>
  void ToCx(
    std::vector<I>* inArray,
    IVector<O>^ outArray) {
    for (auto it = (*inArray).begin(); it != (*inArray).end(); ++it) {
      O outObj;
      ToCx((*it), &outObj);
      outArray->Append(outObj);
    }
  }

  // placeholder functions if no conversion needed (used in vector conversion templates)
  template <typename T>
  void FromCx(T in, T* out) {
    *out = in;
  }

  template <typename T>
  void ToCx(T in, T* out) {
    *out = in;
  }

  // ==========================
  void FromCx(
    webrtc_winrt_api::RTCIceServer^ inObj,
    webrtc::PeerConnectionInterface::IceServer* outObj);

  // ==========================
  void FromCx(
    webrtc_winrt_api::RTCConfiguration^ inObj,
    webrtc::PeerConnectionInterface::RTCConfiguration* outObj);

  // ==========================
  void FromCx(
    webrtc_winrt_api::RTCDataChannelInit^ inObj,
    webrtc::DataChannelInit* outObj);

  // ==========================
  void FromCx(
    webrtc_winrt_api::RTCIceCandidate^ inObj,
    rtc::scoped_ptr<webrtc::IceCandidateInterface>* outObj);
  void ToCx(
    webrtc::IceCandidateInterface const& inObj,
    webrtc_winrt_api::RTCIceCandidate^* outObj);

  // ==========================
  std::string FromCx(
    webrtc_winrt_api::RTCSdpType const& inObj);
  void ToCx(
    std::string const& inObj,
    webrtc_winrt_api::RTCSdpType* outObj);

  // ==========================
  void FromCx(
    webrtc_winrt_api::RTCSessionDescription^ inObj,
    rtc::scoped_ptr<webrtc::SessionDescriptionInterface>* outObj);
  void ToCx(
    const webrtc::SessionDescriptionInterface* inObj,
    webrtc_winrt_api::RTCSessionDescription^* outObj);

  // ==========================
  void ToCx(
    const webrtc::StatsReport* inObj,
    webrtc_winrt_api::RTCStatsReport^* outObj);

}  // namespace webrtc_winrt_api_internal

#endif  // WEBRTC_BUILD_WINRT_GYP_API_MARSHALLING_H_

