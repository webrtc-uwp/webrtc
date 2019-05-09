/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "unity_plugin_apis.h"

#include <map>
#include <string>

#include "simple_peer_connection.h"

namespace {
static int g_peer_connection_id = 1;
static std::map<int, rtc::scoped_refptr<SimplePeerConnection>>
    g_peer_connection_map;
}  // namespace

int CreatePeerConnection(const char** turn_urls,
                         const int no_of_urls,
                         const char* username,
                         const char* credential,
                         bool mandatory_receive_video) {
  g_peer_connection_map[g_peer_connection_id] =
      new rtc::RefCountedObject<SimplePeerConnection>();

  if (!g_peer_connection_map[g_peer_connection_id]->InitializePeerConnection(
          turn_urls, no_of_urls, username, credential, mandatory_receive_video))
    return -1;

  return g_peer_connection_id++;
}

int CreatePeerConnectionWithD3D(const char** turn_urls,
                                const int no_of_urls,
                                const char* username,
                                const char* credential,
                                bool mandatory_receive_video,
                                ID3D11Device* device,
                                ID3D11DeviceContext* context,
                                D3D11_TEXTURE2D_DESC render_target_desc) {
  g_peer_connection_map[g_peer_connection_id] =
      new rtc::RefCountedObject<SimplePeerConnection>();

  //TODO: get the proper description from somewhere. Graphics device is probably
  //a global var in unity, but for spinning cube we can just pass it into this function.

  if (!g_peer_connection_map[g_peer_connection_id]->InitializePeerConnectionWithD3D(
          turn_urls, no_of_urls, username, credential, mandatory_receive_video, device, context, render_target_desc))
    return -1;

  return g_peer_connection_id++;
}

// WEBRTC_PLUGIN_API bool StartD3DSource(int peer_connection_id) {
//   if (!g_peer_connection_map.count(peer_connection_id))
//     return false;

//   g_peer_connection_map[peer_connection_id]->StartD3DSource();
//   return true;
// }

bool ClosePeerConnection(int peer_connection_id) {
  if (!g_peer_connection_map.count(peer_connection_id))
    return false;

  g_peer_connection_map[peer_connection_id]->DeletePeerConnection();
  g_peer_connection_map.erase(peer_connection_id);
  return true;
}

bool AddStream(int peer_connection_id, bool audio_only) {
  if (!g_peer_connection_map.count(peer_connection_id))
    return false;

  g_peer_connection_map[peer_connection_id]->AddStreams(audio_only);
  return true;
}

bool AddDataChannel(int peer_connection_id) {
  if (!g_peer_connection_map.count(peer_connection_id))
    return false;

  return g_peer_connection_map[peer_connection_id]->CreateDataChannel();
}

bool CreateOffer(int peer_connection_id) {
  if (!g_peer_connection_map.count(peer_connection_id))
    return false;

  return g_peer_connection_map[peer_connection_id]->CreateOffer();
}

bool CreateAnswer(int peer_connection_id) {
  if (!g_peer_connection_map.count(peer_connection_id))
    return false;

  return g_peer_connection_map[peer_connection_id]->CreateAnswer();
}

bool SendDataViaDataChannel(int peer_connection_id, const char* data) {
  if (!g_peer_connection_map.count(peer_connection_id))
    return false;

  std::string s(data);
  return g_peer_connection_map[peer_connection_id]->SendDataViaDataChannel(s);

  // return true; TODO: Push fix to google repos
}

bool SetAudioControl(int peer_connection_id, bool is_mute, bool is_record) {
  if (!g_peer_connection_map.count(peer_connection_id))
    return false;

  g_peer_connection_map[peer_connection_id]->SetAudioControl(is_mute,
                                                             is_record);
  return true;
}

bool SetRemoteDescription(int peer_connection_id,
                          const char* type,
                          const char* sdp) {
  if (!g_peer_connection_map.count(peer_connection_id))
    return false;

  return g_peer_connection_map[peer_connection_id]->SetRemoteDescription(type,
                                                                         sdp);
}

bool AddIceCandidate(const int peer_connection_id,
                     const char* candidate,
                     const int sdp_mlineindex,
                     const char* sdp_mid) {
  if (!g_peer_connection_map.count(peer_connection_id))
    return false;

  return g_peer_connection_map[peer_connection_id]->AddIceCandidate(
      candidate, sdp_mlineindex, sdp_mid);
}

bool OnD3DFrame(int peer_connection_id, ID3D11Texture2D* frame) {
  if (!g_peer_connection_map.count(peer_connection_id))
    return false;

  g_peer_connection_map[peer_connection_id]->OnD3DFrame(frame);
  return true;
}

// Register callback functions.
bool RegisterOnLocalI420FrameReady(int peer_connection_id,
                                   I420FRAMEREADY_CALLBACK callback,
                                   void* userData) {
  if (!g_peer_connection_map.count(peer_connection_id))
    return false;

  g_peer_connection_map[peer_connection_id]->RegisterOnLocalI420FrameReady(
      callback,
      userData);
  return true;
}

bool RegisterOnRemoteI420FrameReady(int peer_connection_id,
                                    I420FRAMEREADY_CALLBACK callback,
                                    void* userData) {
  if (!g_peer_connection_map.count(peer_connection_id))
    return false;

  g_peer_connection_map[peer_connection_id]->RegisterOnRemoteI420FrameReady(
      callback,
      userData);
  return true;
}

bool RegisterOnLocalDataChannelReady(int peer_connection_id,
                                     LOCALDATACHANNELREADY_CALLBACK callback, void* userData) {
  if (!g_peer_connection_map.count(peer_connection_id))
    return false;

  g_peer_connection_map[peer_connection_id]->RegisterOnLocalDataChannelReady(
      callback, userData);
  return true;
}

bool RegisterOnDataFromDataChannelReady(
    int peer_connection_id,
    DATAFROMEDATECHANNELREADY_CALLBACK callback,
    void* userData) {
  if (!g_peer_connection_map.count(peer_connection_id))
    return false;

  g_peer_connection_map[peer_connection_id]->RegisterOnDataFromDataChannelReady(
      callback,
      userData);
  return true;
}

bool RegisterOnFailure(int peer_connection_id, FAILURE_CALLBACK callback) {
  if (!g_peer_connection_map.count(peer_connection_id))
    return false;

  g_peer_connection_map[peer_connection_id]->RegisterOnFailure(callback);
  return true;
}

bool RegisterOnAudioBusReady(int peer_connection_id,
                             AUDIOBUSREADY_CALLBACK callback) {
  if (!g_peer_connection_map.count(peer_connection_id))
    return false;

  g_peer_connection_map[peer_connection_id]->RegisterOnAudioBusReady(callback);
  return true;
}

// Singnaling channel related functions.
bool RegisterOnLocalSdpReadytoSend(int peer_connection_id,
                                   LOCALSDPREADYTOSEND_CALLBACK callback,
                                   void* userData) {
  if (!g_peer_connection_map.count(peer_connection_id))
    return false;

  g_peer_connection_map[peer_connection_id]->RegisterOnLocalSdpReadytoSend(
      callback, userData);
  return true;
}

bool RegisterOnIceCandiateReadytoSend(
    int peer_connection_id,
    ICECANDIDATEREADYTOSEND_CALLBACK callback, 
    void* userData) {
  if (!g_peer_connection_map.count(peer_connection_id))
    return false;

  g_peer_connection_map[peer_connection_id]->RegisterOnIceCandiateReadytoSend(
      callback, userData);
  return true;
}
