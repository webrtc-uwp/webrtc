#include "connection.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/peerconnectioninterface.h"
#include "config.h"
#include "logging/rtc_event_log/output/rtc_event_log_output_file.h"
#include "media/base/d3d11_frame_source.h"
#include "media/engine/internaldecoderfactory.h"
#include "media/engine/internalencoderfactory.h"
#include "media/engine/multiplexcodecfactory.h"
#include "rtc_base/asynctcpsocket.h"

#if defined(WEBRTC_WIN)
#include "third_party/winuwp_h264/winuwp_h264_factory.h"

#endif

#if defined(WINUWP)
#include "modules/audio_device/include/fake_audio_device.h"
#endif

namespace hololight::remoting {

Connection::Connection() : weak_ptr_factory_(this) {}

Connection::~Connection() {
  // I guess we should call shutdown from here because this class
  // is managed by scoped_refptr because apparently it inherits from
  // refcountedbase via the interfaces we derive from or such...
  Shutdown();
}

tl::expected<tl::monostate, Error> Connection::Init(
    Config config,
    GraphicsApiConfig graphics_api_config) {
  tl::expected<tl::monostate, Error> result = tl::monostate{};

  InitThreads();

  create_sdp_observer_ = new rtc::RefCountedObject<CreateSdpObserver>(
      weak_ptr_factory_.GetWeakPtr());
  set_sdp_observer_ =
      new rtc::RefCountedObject<SetSdpObserver>(weak_ptr_factory_.GetWeakPtr());

  // TODO: some of these args are based on config and/or platform.
  // e.g. server only needs encoder, client only decoder. Also, these
  // things cannot be changed on the fly for a connection.
  auto video_encoder_result = CreateVideoEncoderFactory(config);
  if (!video_encoder_result.has_value())
    return tl::make_unexpected(video_encoder_result.error());

  // this is kinda sucky because we always need to move a unique_ptr even if
  // constructing from expected
  std::unique_ptr<webrtc::VideoEncoderFactory> video_encoder_factory =
      std::move(video_encoder_result.value());

  auto video_decoder_result = CreateVideoDecoderFactory(config);
  if (!video_decoder_result.has_value())
    return tl::make_unexpected(video_decoder_result.error());

  std::unique_ptr<webrtc::VideoDecoderFactory> video_decoder_factory =
      std::move(video_decoder_result.value());

  rtc::scoped_refptr<webrtc::AudioDeviceModule> adm = CreateADM(config).value();

  peer_connection_factory_ = webrtc::CreatePeerConnectionFactory(
      s_networking_thread_.get(), s_worker_thread_.get(),
      s_signaling_thread_.get(), adm,
      webrtc::CreateBuiltinAudioEncoderFactory(),
      webrtc::CreateBuiltinAudioDecoderFactory(),
      std::move(video_encoder_factory), std::move(video_decoder_factory),
      nullptr,  // audio mixer
      nullptr   // audio processing
  );

  if (peer_connection_factory_.get() == nullptr) {
    return tl::make_unexpected(Error::Type::PeerConnectionFactory);
  }

  RTC_DCHECK(peer_connection_.get() == nullptr);

  // this "config" everywhere sucks balls, we should find a better name
  auto rtc_config = CreateRTCConfiguration(config);

  webrtc::PeerConnectionDependencies pc_deps{
      static_cast<webrtc::PeerConnectionObserver*>(this)};
  peer_connection_ = peer_connection_factory_->CreatePeerConnection(
      rtc_config, nullptr, nullptr, this);

  if (peer_connection_.get() == nullptr) {
    return tl::make_unexpected(Error::Type::PeerConnection);
  } else {
    auto event_log_result = StartEventLog();

    if (!event_log_result.has_value())
      return tl::make_unexpected(event_log_result.error());
    // based on config, instantiate video sources/sinks and add tracks

    // this needs a d3d11 device and context, so init needs them too... :(
    // but only on windows and only when we're server, so it's kinda sucky
    // and needs ifdefs
    auto video_source = CreateVideoTrackSource(config, graphics_api_config);
    if (video_source.has_value() && video_source.value() != nullptr) {
      RTC_LOG(LS_INFO) << "CreateVideoTrackSource succeeded";
      // TODO: track/stream labels should be platform/source-specific
      auto video_track = peer_connection_factory_->CreateVideoTrack(
          "d3d_track", video_source.value());

      auto track = peer_connection_->AddTrack(video_track, {"d3d_video"});

      if (!track.ok()) {
        RTC_LOG(LS_ERROR) << track.error().message();
        return tl::make_unexpected(Error::Type::AddTrack);
      }
    }

    // always create a datachannel. apparently it's reliable by default if I
    // understood right.
    webrtc::DataChannelInit data_channel_init;
    data_channel_ = peer_connection_->CreateDataChannel("input_data_channel",
                                                        &data_channel_init);

    if (data_channel_) {
      data_channel_->RegisterObserver(this);
      RTC_LOG(LS_INFO) << "Init completed successfully";
    } else {
      return tl::make_unexpected(Error::Type::DataChannel);
    }
  }

  return result;
}

tl::expected<tl::monostate, Error> Connection::Init(
    absl::string_view config_path,
    GraphicsApiConfig graphics_api_config) {
  auto config = Config::from_file(config_path);

  return Init(config, graphics_api_config);
}

tl::expected<tl::monostate, Error> Connection::Connect(Config config) {
  // do signaling and everything else until the connection is established.
  // do we want to block here? or are we doing callbacks, etc.?
  // this depends a lot on our role (client vs server) as one creates the offer
  // and the other answers, etc.
  // signaling::factories::SignalingFactory signaling_factory;

  switch (config.role) {
    case Config::Role::Server: {
      // this blocks until we get a connection. I don't think this
      // is desired behavior...but async in c++ is sucky before 17.
      // relay_ = signaling_factory.create_tcp_relay_from_listen(36500);
      // relay_->register_ice_candidate_handler(
      //     [](hololight::signaling::webrtc::IceCandidate candidate) {
      //       // do shit when we receive ice candidates from other side
      //     });
      // relay_->register_sdp_handler([](hololight::signaling::webrtc::Sdp sdp)
      // {
      //   // do shit when we receive the remote description
      // });
      // here we could continue with the whole offer/answer bs
      // instead of creating the track when creating the peerconnection, we
      // could do that here and kick off the whole offer/answer stuff in
      // OnRenegotiationNeeded.
      const webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;

      // peer_connection_->CreateOffer(this, options);

      // rtc::ThreadManager::Instance()->CurrentThread()->SleepMs(5000);

      // TODO: create socket and listen
      if (listen_socket_ == nullptr) {
      }
      listen_socket_ = std::unique_ptr<rtc::AsyncSocket>(
          s_networking_thread_->socketserver()->CreateAsyncSocket(AF_INET,
                                                                  SOCK_STREAM));
      // rtc::AsyncTCPSocket* tcp_socket = new rtc::AsyncTCPSocket(async_socket,
      // /*listen:*/true);

      // signaling_socket_ = std::unique_ptr<rtc::AsyncSocket>();
      rtc::SocketAddress addr = rtc::SocketAddress("192.168.111.149", 9999);
      if (!listen_socket_->Bind(addr)) {
        RTC_LOG(LS_ERROR) << "Socket bind failed, abandon ship!";
      }

      if (!listen_socket_->Listen(5)) {
        RTC_LOG(LS_ERROR) << "Socket listen failed, oh no!";
      }

      // listen_socket_->SignalReadEvent.connect()

      break;
    }
    case Config::Role::Client: {
      // do client stuff, connect to signaling, create answer, etc.

      // relay_ = signaling_factory.create_tcp_relay_from_connect(
      //     "192.168.111.119", 36500);
      // relay_->register_ice_candidate_handler(
      //     [](hololight::signaling::webrtc::IceCandidate candidate) {
      //       // do shit when we receive ice candidates from other side
      //     });
      // relay_->register_sdp_handler([](hololight::signaling::webrtc::Sdp sdp)
      // {
      //   // do shit when we receive the remote description
      // });

      // we need to poll for messages somewhere, but there is no main loop here
      // (yet?) would a callback-based api work better for us? I mean
      // peerconnection is all callbacks too.

      auto async_socket =
          s_networking_thread_->socketserver()->CreateAsyncSocket(AF_INET,
                                                                  SOCK_STREAM);
      auto bind_addr = rtc::SocketAddress("0.0.0.0", 9999);
      auto remote_addr = rtc::SocketAddress("192.168.0.1", 9999);
      auto tcp_socket =
          rtc::AsyncTCPSocket::Create(async_socket, bind_addr, remote_addr);
      signaling_socket_ = std::unique_ptr<rtc::AsyncTCPSocket>(tcp_socket);

      // TODO:
      // signaling_socket_->SignalConnect
      // signaling_socket_->SignalClose
      // signaling_socket_->SignalReadPacket
    } break;
    default:
      RTC_NOTREACHED() << "Config value != client or server";
      break;
  }

  return tl::monostate{};
}

void Connection::OnAccept(rtc::AsyncSocket* listen_socket) {
  rtc::AsyncSocket* raw_socket = listen_socket->Accept(nullptr);
  if (raw_socket) {
    signaling_socket_ =
        std::make_unique<rtc::AsyncTCPSocket>(raw_socket, false);
    // packet_socket->SignalReadPacket.connect(this, &TestEchoServer::OnPacket);
    // packet_socket->SignalClose.connect(this, &TestEchoServer::OnClose);
    // client_sockets_.push_back(packet_socket);
  }
}

void Connection::OnPacket(rtc::AsyncPacketSocket* socket,
                          const char* buf,
                          size_t size,
                          const rtc::SocketAddress& remote_addr,
                          const rtc::PacketTime& packet_time) {
  // yeah, raw buffers. awesome! (not)
  // we could wrap it in arrayview so it's less of a pain in the ass
}

void Connection::OnClose(rtc::AsyncPacketSocket* socket, int err) {}

tl::expected<tl::monostate, Error> Connection::StartEventLog() {
  RTC_DCHECK(peer_connection_.get() != nullptr);

  // instead of fucking around with programmatically getting a path on winuwp,
  // use the ms-appdata:///local/ uri. more info here:
  // https://docs.microsoft.com/en-us/windows/uwp/app-resources/uri-schemes#ms-appdata
  const uint32_t max_file_size =
      10 * 1024 * 1024;  // 10MiB max size for event log
#ifdef WINUWP
  const std::string log_file_path = "ms-appdata:///local/rtc_event.log";
#else
  const std::string log_file_path = "rtc_event.log";
#endif

  int64_t output_period_ms = 100;

  if (!peer_connection_->StartRtcEventLog(
          absl::make_unique<webrtc::RtcEventLogOutputFile>(log_file_path,
                                                           max_file_size),
          output_period_ms)) {
    RTC_LOG(LS_ERROR) << "Failed to start event log";
    return tl::make_unexpected(Error::Type::StartRtcEventLog);
  }

  return tl::monostate{};
}

void Connection::StopEventLog() {
  RTC_DCHECK(peer_connection_.get() != nullptr);

  peer_connection_->StopRtcEventLog();
}

tl::expected<rtc::scoped_refptr<webrtc::VideoTrackSourceInterface>, Error>
Connection::CreateVideoTrackSource(Config config,
                                   GraphicsApiConfig gfx_api_config) {
  switch (config.video_source) {
#if defined(WEBRTC_WIN)
    case Config::VideoSource::D3D11: {
      auto source = D3D11VideoFrameSource::Create(
          gfx_api_config.d3d_device, gfx_api_config.d3d_context,
          gfx_api_config.render_target_desc, s_signaling_thread_.get());
      if (source) {
        return source;
      } else {
        tl::make_unexpected(Error::Type::VideoSource);
      }
    }
#endif
    case Config::VideoSource::Webcam:
      return tl::make_unexpected(Error::Type::VideoSource);
    case Config::VideoSource::None:
      // should we use std::optional instead?
      return nullptr;
    default:
      return tl::make_unexpected(Error::Type::UnsupportedConfig);
  }
}

void Connection::InitThreads() {
  // maybe we should return an error here instead of dying in lib code.
  // otoh, if we are unable to create threads this is useless anyway...

  // android jni peerconnectionfactory.cc uses 3 threads, so we do too even
  // though unityplugin did it differently (using deprecated thread constructor)
  s_signaling_thread_ = rtc::Thread::Create();
  RTC_CHECK(s_signaling_thread_->SetName("signaling_thread", nullptr));
  RTC_CHECK(s_signaling_thread_->Start());

  s_worker_thread_ = rtc::Thread::Create();
  RTC_CHECK(s_worker_thread_->SetName("worker_thread", nullptr));
  RTC_CHECK(s_worker_thread_->Start());

  s_networking_thread_ = rtc::Thread::CreateWithSocketServer();
  RTC_CHECK(s_networking_thread_->SetName("networking_thread", nullptr));
  RTC_CHECK(s_networking_thread_->Start());
}

webrtc::PeerConnectionInterface::RTCConfiguration
Connection::CreateRTCConfiguration(Config config) {
  webrtc::PeerConnectionInterface::RTCConfiguration rtc_config;
  webrtc::PeerConnectionInterface::IceServer server;
  server.urls.insert(server.urls.cend(), config.ice_servers.begin(),
                     config.ice_servers.end());
  server.urls.push_back("stun:stun.l.google.com:19302");
  rtc_config.servers.push_back(server);
  rtc_config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
  // rtc_config.continual_gathering_policy =
  //     webrtc::PeerConnectionInterface::GATHER_CONTINUALLY;
  // rtc_config.ice_regather_interval_range = absl::nullopt;
  // rtc_config.enable_dtls_srtp = true;

  return rtc_config;
}

tl::expected<std::unique_ptr<webrtc::VideoEncoderFactory>, Error>
Connection::CreateVideoEncoderFactory(Config config) {
  switch (config.encoder) {
    case Config::Encoder::None:
      // TODO: test this. not sure if we can pass nullptr to pc factory
      return nullptr;
    case Config::Encoder::Builtin:
      return std::unique_ptr<webrtc::VideoEncoderFactory>(
          new webrtc::MultiplexEncoderFactory(
              absl::make_unique<webrtc::InternalEncoderFactory>()));
    case Config::Encoder::H264_UWP:
      return std::unique_ptr<webrtc::VideoEncoderFactory>(
          new webrtc::WinUWPH264EncoderFactoryNew());
    default:
      return tl::make_unexpected(Error::Type::UnsupportedConfig);
  }
}

tl::expected<std::unique_ptr<webrtc::VideoDecoderFactory>, Error>
Connection::CreateVideoDecoderFactory(Config config) {
  switch (config.decoder) {
    case Config::Decoder::None:
      // TODO: test this. not sure if we can pass nullptr to pc factory
      return nullptr;
    case Config::Decoder::Builtin:
      std::unique_ptr<webrtc::VideoDecoderFactory>(
          new webrtc::MultiplexDecoderFactory(
              absl::make_unique<webrtc::InternalDecoderFactory>()));
    default:
      return tl::make_unexpected(Error::Type::UnsupportedConfig);
  }
}

tl::expected<rtc::scoped_refptr<webrtc::AudioDeviceModule>, Error>
Connection::CreateADM(Config config) {
#if defined(WINUWP)
  // this should eventually be replaced by a functioning adm
  // from the uwp wrapper
  auto fake_adm_ptr =
      new rtc::RefCountedObject<webrtc::FakeAudioDeviceModule>();
  return fake_adm_ptr;
#endif
  // this creates a default adm when passed into createpeerconnectionfactory
  return nullptr;
}

void Connection::Shutdown() {
  if (data_channel_.get() != nullptr) {
    data_channel_->UnregisterObserver();
    data_channel_->Close();
    data_channel_ = nullptr;
  }

  if (peer_connection_.get() != nullptr) {
    StopEventLog();
    // not sure if we need to remove the track or source before closing this

    peer_connection_->Close();
    peer_connection_ = nullptr;
  }

  if (peer_connection_factory_.get() != nullptr) {
    peer_connection_factory_ = nullptr;
  }

  // throw everything away!
}

void Connection::OnTrack(
    rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver) {
  // this is called on the client when we get a new track (after setting remote
  // description)
}

void Connection::OnSignalingChange(
    webrtc::PeerConnectionInterface::SignalingState new_state) {
  RTC_LOG(LS_INFO) << "SignalingState changed to " << new_state
                   << ", called from "
                   << rtc::ThreadManager::Instance()->CurrentThread()->name();
}
void Connection::OnDataChannel(
    rtc::scoped_refptr<webrtc::DataChannelInterface> channel) {
  // this seems to be used only on the client side because otherwise we create
  // the datachannel ourselves. should this connection class handle both cases,
  // though? I mean if a rogue client opens a datachannel we'd essentially
  // overwrite our own as the server, which would kinda suck.
  this->data_channel_ = channel;
}
void Connection::OnRenegotiationNeeded() {
  // do the whole offer/answer bs again

  // in web browsers this is actually triggered when adding a track to the
  // connection. not sure how it is done in this lib.
  // It seems to be the same. Thing is, we want the user to trigger the
  // connection process, not the addition of a track, so this callback will
  // likely end up doing nothing.
  // This also happens when adding a datachannel, but we should only react to
  // this when there's a track or something on the server side
  RTC_LOG(LS_INFO) << "Renegotiation triggered (called from "
                   << rtc::ThreadManager::Instance()->CurrentThread()->name()
                   << ")";
}
void Connection::OnIceConnectionChange(
    webrtc::PeerConnectionInterface::IceConnectionState new_state) {
  RTC_LOG(LS_INFO) << "ICE connection state changed to " << new_state;
}
void Connection::OnIceGatheringChange(
    webrtc::PeerConnectionInterface::IceGatheringState new_state) {
  RTC_LOG(LS_INFO) << "ICE gathering state changed to " << new_state;
}
void Connection::OnIceCandidate(
    const webrtc::IceCandidateInterface* candidate) {
  // TODO: add ICE candidate here
  // peer_connection_->AddIceCandidate(candidate);
  RTC_LOG(LS_INFO) << "OnIceCandidate called";
}

void Connection::OnCreateSdpSuccess(webrtc::SessionDescriptionInterface* desc) {
  // called when createoffer/createanswer succeeds
  RTC_LOG(LS_INFO) << "SDP OnSuccess called from "
                   << rtc::ThreadManager::Instance()->CurrentThread()->name();
  // std::string sdp;
  // desc->ToString(&sdp);
  // RTC_LOG(LS_INFO) << sdp;

  // peer_connection_->SetLocalDescription(nullptr, desc);
}
void Connection::OnCreateSdpFailure(webrtc::RTCError error) {
  RTC_LOG(LS_ERROR) << error.message();
}

void Connection::OnSetSdpSuccess() {
  // called when createoffer/createanswer succeeds
  RTC_LOG(LS_INFO) << "SDP OnSuccess called from "
                   << rtc::ThreadManager::Instance()->CurrentThread()->name();
}

void Connection::OnSetSdpFailure(webrtc::RTCError error) {
  RTC_LOG(LS_ERROR) << error.message();
}

// DataChannelObserver implementation.
void Connection::OnStateChange() {
  RTC_LOG(LS_INFO) << "DataChannel state changed";
}
void Connection::OnMessage(const webrtc::DataBuffer& buffer) {}
}  // namespace hololight::remoting