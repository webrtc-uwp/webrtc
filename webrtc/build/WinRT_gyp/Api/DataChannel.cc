
// Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <vector>

#include "webrtc/build/WinRT_gyp/Api/DataChannel.h"
#include "Marshalling.h"

using Org::WebRtc::Internal::ToCx;

namespace Org.WebRtc {

RTCDataChannel::RTCDataChannel(
  rtc::scoped_refptr<webrtc::DataChannelInterface> impl)
  : _impl(impl) {
}

rtc::scoped_refptr<webrtc::DataChannelInterface> RTCDataChannel::GetImpl() {
  return _impl;
}

String^ RTCDataChannel::Label::get() {
  return ToCx(_impl->label());
}

bool RTCDataChannel::Ordered::get() {
  return _impl->ordered();
}

IBox<uint16>^ RTCDataChannel::MaxPacketLifeTime::get() {
  return _impl->maxRetransmitTime() != -1
    ? _impl->maxRetransmitTime()
    : (IBox<uint16>^)nullptr;
}

IBox<uint16>^ RTCDataChannel::MaxRetransmits::get() {
  return _impl->maxRetransmits() != -1
    ? _impl->maxRetransmits()
    : (IBox<uint16>^)nullptr;
}

String^ RTCDataChannel::Protocol::get() {
  return ToCx(_impl->protocol());
}

bool RTCDataChannel::Negotiated::get() {
  return _impl->negotiated();
}

uint16 RTCDataChannel::Id::get() {
  return _impl->id();
}

void RTCDataChannel::Close() {
  _impl->Close();
}

RTCDataChannelState RTCDataChannel::ReadyState::get() {
  RTCDataChannelState state;
  ToCx(_impl->state(), &state);
  return state;
}

StringDataChannelMessage::StringDataChannelMessage(String^ data) {
  StringData = data;
}

BinaryDataChannelMessage::BinaryDataChannelMessage(IVector<byte>^ data) {
  BinaryData = data;
}

unsigned int RTCDataChannel::BufferedAmount::get() {
  return _impl->buffered_amount();
}

void RTCDataChannel::Send(IDataChannelMessage^ message) {
  if (message->DataType == RTCDataChannelMessageType::String) {
    StringDataChannelMessage^ stringMessage =
        (StringDataChannelMessage^)message;

    webrtc::DataBuffer buffer(rtc::ToUtf8(stringMessage->StringData->Data()));
    _impl->Send(buffer);
  } else if (message->DataType == RTCDataChannelMessageType::Binary) {
    BinaryDataChannelMessage^ binaryMessage =
        (BinaryDataChannelMessage^)message;

    std::vector<byte> binaryDataVector;
    binaryDataVector.reserve(binaryMessage->BinaryData->Size);

    // convert IVector to std::vector
		Org::WebRtc::Internal::FromCx(binaryMessage->BinaryData,
        &binaryDataVector);

    byte* byteArr = (&binaryDataVector[0]);
    const rtc::Buffer rtcBuffer(byteArr, binaryDataVector.size());
    webrtc::DataBuffer buffer(rtcBuffer, true);

    _impl->Send(buffer);
  } else {
    LOG(LS_ERROR) << "Tried to send data channel message of unknown data type";
  }
}

}  // namespace Org.WebRtc
