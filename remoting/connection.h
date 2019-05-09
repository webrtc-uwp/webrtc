#ifndef HOLOLIGHT_REMOTING_CONNECTION_H
#define HOLOLIGHT_REMOTING_CONNECTION_H

#include "absl/strings/string_view.h"
#include "api/peerconnectioninterface.h"
#include "config.h"
#include "error.h"
#include "rtc_base/asynctcpsocket.h"
#include "sdp_observers.h"
#include "tl/expected.hpp"

#if defined(WEBRTC_WIN)
#include <d3d11.h>
#include "media/base/d3d11_frame_source.h"

// TODO: move out as soon as we replace the uwp-specific socket code
#include "signaling/factories/signaling_factory.h"
#include "signaling/signaling_relay.h"
#endif

namespace hololight::remoting {

// interestingly, this isn't refcounted when deriving from
// peerconnectionobserver and datachannelobserver. it was the sdp observers that
// fucked shit up.
class Connection : public webrtc::PeerConnectionObserver,
                   public webrtc::DataChannelObserver {
 public:
  Connection();
  ~Connection();

  tl::expected<tl::monostate, Error> Init(
      absl::string_view config_path,
      GraphicsApiConfig graphics_api_config);
  tl::expected<tl::monostate, Error> Init(
      Config config,
      GraphicsApiConfig graphics_api_config);
  tl::expected<tl::monostate, Error> Connect(Config config);

  void Shutdown();

  // PeerConnectionObserver implementation.
  void OnSignalingChange(
      webrtc::PeerConnectionInterface::SignalingState new_state) override;
  void OnDataChannel(
      rtc::scoped_refptr<webrtc::DataChannelInterface> channel) override;
  void OnRenegotiationNeeded() override;
  void OnIceConnectionChange(
      webrtc::PeerConnectionInterface::IceConnectionState new_state) override;
  void OnIceGatheringChange(
      webrtc::PeerConnectionInterface::IceGatheringState new_state) override;
  void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override;

  void OnTrack(
      rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver) override;

  // void OnIceConnectionReceivingChange(bool receiving) override;

  // DataChannelObserver implementation.
  void OnStateChange() override;
  void OnMessage(const webrtc::DataBuffer& buffer) override;

 private:
  void InitThreads();

  // we could create a connection builder which methods can only be called in a
  // specific order e.g.
  // builder.with_config(config).create_threads().create_video_track_source()...
  // etc.
  tl::expected<rtc::scoped_refptr<webrtc::VideoTrackSourceInterface>, Error>
  CreateVideoTrackSource(Config config, GraphicsApiConfig gfx_api_config);

  tl::expected<std::unique_ptr<webrtc::VideoEncoderFactory>, Error>
  CreateVideoEncoderFactory(Config config);
  tl::expected<std::unique_ptr<webrtc::VideoDecoderFactory>, Error>
  CreateVideoDecoderFactory(Config config);
  tl::expected<rtc::scoped_refptr<webrtc::AudioDeviceModule>, Error> CreateADM(
      Config config);
  webrtc::PeerConnectionInterface::RTCConfiguration CreateRTCConfiguration(
      Config config);
  tl::expected<tl::monostate, Error> StartEventLog();
  void StopEventLog();

  void OnCreateSdpSuccess(webrtc::SessionDescriptionInterface* desc);
  void OnCreateSdpFailure(webrtc::RTCError error);
  void OnSetSdpSuccess();
  void OnSetSdpFailure(webrtc::RTCError error);

  void OnAccept(rtc::AsyncSocket* listen_socket);
  void OnPacket(rtc::AsyncPacketSocket* socket,
                const char* buf,
                size_t size,
                const rtc::SocketAddress& remote_addr,
                const rtc::PacketTime& packet_time);
  void OnClose(rtc::AsyncPacketSocket* socket, int err);

  // TODO: drop the s_ prefix, we're not static anymore (thank god)
  std::unique_ptr<rtc::Thread> s_signaling_thread_;
  std::unique_ptr<rtc::Thread> s_worker_thread_;
  std::unique_ptr<rtc::Thread> s_networking_thread_;

  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>
      peer_connection_factory_;
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
  rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel_;

  // TODO: ifdef this out or hide behind something
  //   rtc::scoped_refptr<hololight::D3D11VideoFrameSource>
  //       d3d11_video_frame_source_;

  std::unique_ptr<rtc::AsyncSocket> listen_socket_;
  std::unique_ptr<rtc::AsyncTCPSocket> signaling_socket_;

  std::unique_ptr<hololight::signaling::webrtc::WebrtcSignalingRelay> relay_;

  rtc::scoped_refptr<CreateSdpObserver> create_sdp_observer_;
  rtc::scoped_refptr<SetSdpObserver> set_sdp_observer_;
  // all members should be above this so they are not invalidated before weak
  // pointers are
  rtc::WeakPtrFactory<Connection> weak_ptr_factory_;
};

}  // namespace hololight::remoting

#endif  // HOLOLIGHT_REMOTING_CONNECTION_H