/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef EXAMPLES_REMOTING_SIMPLE_PEER_CONNECTION_H_
#define EXAMPLES_REMOTING_SIMPLE_PEER_CONNECTION_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "api/datachannelinterface.h"
#include "api/mediastreaminterface.h"
#include "api/peerconnectioninterface.h"
#include "unity_plugin_apis.h"
#include "video_observer.h"

#if WEBRTC_WIN
#include <d3d11.h>
#include <winrt/base.h>
#include "media/base/d3d11_frame_source.h"
#endif

class StatsCollector : public webrtc::RTCStatsCollectorCallback {
  void OnStatsDelivered(
      const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report) override {
    RTC_LOG(LS_INFO) << report->ToJson();
  }
};

class SimplePeerConnection : public webrtc::PeerConnectionObserver,
                             public webrtc::CreateSessionDescriptionObserver,
                             public webrtc::DataChannelObserver,
                             public webrtc::AudioTrackSinkInterface {
 public:
  SimplePeerConnection() {}
  ~SimplePeerConnection() {}

  bool InitializePeerConnection(const char** turn_urls,
                                const int no_of_urls,
                                const char* username,
                                const char* credential,
                                bool is_receiver);

  bool InitializePeerConnectionWithD3D(const char** turn_urls,
                                       const int no_of_urls,
                                       const char* username,
                                       const char* credential,
                                       bool is_receiver,
                                       ID3D11Device* device,
                                       ID3D11DeviceContext* context,
                                       D3D11_TEXTURE2D_DESC render_target_desc);

  void StartD3DSource();
  void DeletePeerConnection();
  void AddStreams(bool audio_only);
  bool CreateDataChannel();
  bool CreateOffer();
  bool CreateAnswer();
  bool SendDataViaDataChannel(const std::string& data);
  void SetAudioControl(bool is_mute, bool is_record);

  void OnD3DFrame(ID3D11Texture2D* rendered_image);

  // Register callback functions.
  void RegisterOnLocalI420FrameReady(I420FRAMEREADY_CALLBACK callback,
                                     void* userData);
  void RegisterOnRemoteI420FrameReady(I420FRAMEREADY_CALLBACK callback,
                                      void* userData);
  void RegisterOnLocalDataChannelReady(LOCALDATACHANNELREADY_CALLBACK callback,
                                       void* userData);
  void RegisterOnDataFromDataChannelReady(
      DATAFROMEDATECHANNELREADY_CALLBACK callback,
      void* userData);
  void RegisterOnFailure(FAILURE_CALLBACK callback);
  void RegisterOnAudioBusReady(AUDIOBUSREADY_CALLBACK callback);
  void RegisterOnLocalSdpReadytoSend(LOCALSDPREADYTOSEND_CALLBACK callback,
                                     void* userData);
  void RegisterOnIceCandiateReadytoSend(
      ICECANDIDATEREADYTOSEND_CALLBACK callback,
      void* userData);
  bool SetRemoteDescription(const char* type, const char* sdp);
  bool AddIceCandidate(const char* sdp,
                       const int sdp_mlineindex,
                       const char* sdp_mid);

 protected:
  // create a peerconneciton and add the turn servers info to the configuration.
  bool CreatePeerConnection(const char** turn_urls,
                            const int no_of_urls,
                            const char* username,
                            const char* credential);
  void CloseDataChannel();
  std::unique_ptr<cricket::VideoCapturer> OpenVideoCaptureDevice();
  void SetAudioControl();

  // PeerConnectionObserver implementation.
  void OnSignalingChange(
      webrtc::PeerConnectionInterface::SignalingState new_state) override {}
  void OnAddStream(
      rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override;
  void OnRemoveStream(
      rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override {}
  void OnDataChannel(
      rtc::scoped_refptr<webrtc::DataChannelInterface> channel) override;
  void OnRenegotiationNeeded() override {}
  void OnIceConnectionChange(
      webrtc::PeerConnectionInterface::IceConnectionState new_state) override {}
  void OnIceGatheringChange(
      webrtc::PeerConnectionInterface::IceGatheringState new_state) override {}
  void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override;
  void OnIceConnectionReceivingChange(bool receiving) override {}

  // CreateSessionDescriptionObserver implementation.
  void OnSuccess(webrtc::SessionDescriptionInterface* desc) override;
  void OnFailure(webrtc::RTCError error) override;

  // DataChannelObserver implementation.
  void OnStateChange() override;
  void OnMessage(const webrtc::DataBuffer& buffer) override;

  // AudioTrackSinkInterface implementation.
  void OnData(const void* audio_data,
              int bits_per_sample,
              int sample_rate,
              size_t number_of_channels,
              size_t number_of_frames) override;

  // Get remote audio tracks ssrcs.
  std::vector<uint32_t> GetRemoteAudioTrackSsrcs();

 private:
  std::string GetLogFilePath();

  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
  rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel_;
  std::map<std::string, rtc::scoped_refptr<webrtc::MediaStreamInterface> >
      active_streams_;

  std::unique_ptr<VideoObserver> local_video_observer_;
  std::unique_ptr<VideoObserver> remote_video_observer_;

  rtc::scoped_refptr<hololight::D3D11VideoFrameSource> local_d3d_track_source_;
  rtc::scoped_refptr<StatsCollector> stats_observer_;

  webrtc::MediaStreamInterface* remote_stream_ = nullptr;
  webrtc::PeerConnectionInterface::RTCConfiguration config_;

  LOCALDATACHANNELREADY_CALLBACK OnLocalDataChannelReady = nullptr;
  DATAFROMEDATECHANNELREADY_CALLBACK OnDataFromDataChannelReady = nullptr;
  FAILURE_CALLBACK OnFailureMessage = nullptr;
  AUDIOBUSREADY_CALLBACK OnAudioReady = nullptr;

  LOCALSDPREADYTOSEND_CALLBACK OnLocalSdpReady = nullptr;
  ICECANDIDATEREADYTOSEND_CALLBACK OnIceCandiateReady = nullptr;

  bool is_mute_audio_ = false;
  bool is_record_audio_ = false;
  bool mandatory_receive_ = false;

  // disallow copy-and-assign
  SimplePeerConnection(const SimplePeerConnection&) = delete;
  SimplePeerConnection& operator=(const SimplePeerConnection&) = delete;

  // this will make android builds fail for now, but somewhere down the road
  // we'll have an abstraction for it.
  winrt::com_ptr<ID3D11Device> d3d_device_;
  winrt::com_ptr<ID3D11DeviceContext> d3d_context_;
  D3D11_TEXTURE2D_DESC d3d_render_target_desc_;

  // Userdata for passing back to callbacks. Enables them to call instanced
  // functions
  void* local_datachannel_ready_callback_;
  void* local_sdp_callback_userdata_;
  void* ice_candidate_send_userdata_;
  void* on_datachannel_data_ready_userdata_;
};

#endif  // EXAMPLES_REMOTING_SIMPLE_PEER_CONNECTION_H_
