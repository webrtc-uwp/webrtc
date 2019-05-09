/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "simple_peer_connection.h"

#include <codecvt>
#include <locale>
#include <utility>

#include "absl/memory/memory.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/stats/rtcstatscollectorcallback.h"
#include "api/stats/rtcstatsreport.h"
#include "api/videosourceproxy.h"
#include "logging/rtc_event_log/output/rtc_event_log_output_file.h"
#include "media/engine/internaldecoderfactory.h"
#include "media/engine/internalencoderfactory.h"
#include "media/engine/multiplexcodecfactory.h"
#include "media/engine/webrtcvideocapturerfactory.h"
#include "media/engine/webrtcvideodecoderfactory.h"
#include "media/engine/webrtcvideoencoderfactory.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "modules/video_capture/video_capture_factory.h"

#if defined(WEBRTC_ANDROID)
#include "examples/remoting/classreferenceholder.h"
#include "modules/utility/include/helpers_android.h"
#include "sdk/android/src/jni/androidvideotracksource.h"
#include "sdk/android/src/jni/jni_helpers.h"
#endif

#if defined(WEBRTC_WIN)
#include <winrt/Windows.Storage.h>
#include "media/base/d3d11_frame_source.h"
#include "modules/audio_device/include/fake_audio_device.h"
#include "third_party/winuwp_h264/winuwp_h264_factory.h"
#endif

// Names used for media stream ids.
const char kAudioLabel[] = "audio_label";
const char kVideoLabel[] = "video_label";
const char kStreamId[] = "stream_id";

namespace {
static int g_peer_count = 0;
static std::unique_ptr<rtc::Thread> g_worker_thread;
static std::unique_ptr<rtc::Thread> g_signaling_thread;
static rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>
    g_peer_connection_factory;
#if defined(WEBRTC_ANDROID)
// Android case: the video track does not own the capturer, and it
// relies on the app to dispose the capturer when the peerconnection
// shuts down.
static jobject g_camera = nullptr;
#endif

std::string GetEnvVarOrDefault(const char* env_var_name,
                               const char* default_value) {
  std::string value;
  const char* env_var = getenv(env_var_name);
  if (env_var)
    value = env_var;

  if (value.empty())
    value = default_value;

  return value;
}

// stolen from
// https://stackoverflow.com/questions/440133/how-do-i-create-a-random-alpha-numeric-string-in-c
// move it somewhere else sometime
std::wstring random_string(size_t length) {
  auto randchar = []() -> char {
    const char charset[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    const size_t max_index = (sizeof(charset) - 1);
    return charset[rand() % max_index];
  };
  std::wstring str(length, 0);
  std::generate_n(str.begin(), length, randchar);
  return str;
}

std::string GetPeerConnectionString() {
  return GetEnvVarOrDefault("WEBRTC_CONNECT", "stun:stun.l.google.com:19302");
}

class DummySetSessionDescriptionObserver
    : public webrtc::SetSessionDescriptionObserver {
 public:
  static DummySetSessionDescriptionObserver* Create() {
    return new rtc::RefCountedObject<DummySetSessionDescriptionObserver>();
  }
  virtual void OnSuccess() { RTC_LOG(INFO) << __FUNCTION__; }
  virtual void OnFailure(webrtc::RTCError error) {
    RTC_LOG(INFO) << __FUNCTION__ << " " << ToString(error.type()) << ": "
                  << error.message();
  }

 protected:
  DummySetSessionDescriptionObserver() {}
  ~DummySetSessionDescriptionObserver() {}
};

}  // namespace

bool SimplePeerConnection::InitializePeerConnection(const char** turn_urls,
                                                    const int no_of_urls,
                                                    const char* username,
                                                    const char* credential,
                                                    bool is_receiver) {
  RTC_DCHECK(peer_connection_.get() == nullptr);

  if (g_peer_connection_factory == nullptr) {
    g_worker_thread.reset(new rtc::Thread());
    g_worker_thread->Start();
    g_signaling_thread.reset(new rtc::Thread());
    g_signaling_thread->Start();

    // soo, there's no uwp support outside the wrapper, which sucks for us.
    // solution: fake it till you make it.
    auto fake_adm = new rtc::RefCountedObject<webrtc::FakeAudioDeviceModule>();
    webrtc::WinUWPH264EncoderFactoryNew* encoder_factory =
        new webrtc::WinUWPH264EncoderFactoryNew();
    webrtc::WinUWPH264DecoderFactoryNew* decoder_factory =
        new webrtc::WinUWPH264DecoderFactoryNew();

    g_peer_connection_factory = webrtc::CreatePeerConnectionFactory(
        g_worker_thread.get(), g_worker_thread.get(), g_signaling_thread.get(),
        fake_adm, webrtc::CreateBuiltinAudioEncoderFactory(),
        webrtc::CreateBuiltinAudioDecoderFactory(),
        std::unique_ptr<webrtc::VideoEncoderFactory>(
            /*new webrtc::MultiplexEncoderFactory(
                absl::make_unique<webrtc::InternalEncoderFactory>()))*/
            encoder_factory),
        std::unique_ptr<webrtc::VideoDecoderFactory>(
            /*new webrtc::MultiplexDecoderFactory(
                absl::make_unique<webrtc::InternalDecoderFactory>()))*/
            decoder_factory),
        nullptr, nullptr);
  }
  if (!g_peer_connection_factory.get()) {
    DeletePeerConnection();
    return false;
  }

  g_peer_count++;
  if (!CreatePeerConnection(turn_urls, no_of_urls, username, credential)) {
    DeletePeerConnection();
    return false;
  }

  uint32_t maxFileSize = 10000000;
  std::string logFilePath = GetLogFilePath();
  int64_t outputPeriodMs = 16 * 60;  // every 60th frame assuming 16ms frame
                                     // time

  if (!peer_connection_->StartRtcEventLog(
          absl::make_unique<webrtc::RtcEventLogOutputFile>(logFilePath,
                                                           maxFileSize),
          outputPeriodMs))
    RTC_LOG(LS_ERROR) << "Failed to start event log";

  mandatory_receive_ = is_receiver;
  return peer_connection_.get() != nullptr;
}

std::string SimplePeerConnection::GetLogFilePath() {
#ifdef WINUWP
  // on winrt, get package local path
  auto path =
      winrt::Windows::Storage::ApplicationData::Current().LocalFolder().Path();
  std::wstring wstr =
      std::wstring(path.c_str()) + L"\\rtc_event" + random_string(7) + L".log";
#else
  std::wstring wstr =
      std::wstring(L"rtc_event" + random_string(7) + L".log");
#endif

  // code from
  // https://stackoverflow.com/questions/4804298/how-to-convert-wstring-into-string
  using convert_type = std::codecvt_utf8<wchar_t>;
  std::wstring_convert<convert_type, wchar_t> converter;

  return converter.to_bytes(wstr);
}

void SimplePeerConnection::StartD3DSource() {
  local_d3d_track_source_->SetState(
      rtc::AdaptedVideoTrackSource::SourceState::kLive);
}

bool SimplePeerConnection::InitializePeerConnectionWithD3D(
    const char** turn_urls,
    const int no_of_urls,
    const char* username,
    const char* credential,
    bool is_receiver,
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    D3D11_TEXTURE2D_DESC render_target_desc) {
  RTC_DCHECK(peer_connection_.get() == nullptr);
  RTC_CHECK(device != nullptr);
  RTC_CHECK(context != nullptr);
  d3d_device_.copy_from(device);
  d3d_context_.copy_from(context);
  d3d_render_target_desc_ = render_target_desc;

  if (g_peer_connection_factory == nullptr) {
    g_worker_thread.reset(new rtc::Thread());
    g_worker_thread->Start();
    g_signaling_thread.reset(new rtc::Thread());
    g_signaling_thread->Start();

    // this is here so we can print stats when the connection is deleted
    stats_observer_ = new rtc::RefCountedObject<StatsCollector>();

    // soo, there's no uwp support outside the wrapper, which sucks for us.
    // solution: fake it till you make it.
    auto fake_adm = new rtc::RefCountedObject<webrtc::FakeAudioDeviceModule>();
    webrtc::WinUWPH264EncoderFactoryNew* encoder_factory =
        new webrtc::WinUWPH264EncoderFactoryNew();
    webrtc::WinUWPH264DecoderFactoryNew* decoder_factory =
        new webrtc::WinUWPH264DecoderFactoryNew();

    g_peer_connection_factory = webrtc::CreatePeerConnectionFactory(
        g_worker_thread.get(), g_worker_thread.get(), g_signaling_thread.get(),
        fake_adm, webrtc::CreateBuiltinAudioEncoderFactory(),
        webrtc::CreateBuiltinAudioDecoderFactory(),
        std::unique_ptr<webrtc::VideoEncoderFactory>(
            /*new webrtc::MultiplexEncoderFactory(
                absl::make_unique<webrtc::InternalEncoderFactory>())*/
            encoder_factory),
        std::unique_ptr<webrtc::VideoDecoderFactory>(
            /*new webrtc::MultiplexDecoderFactory(
                absl::make_unique<webrtc::InternalDecoderFactory>()))*/
            decoder_factory),
        nullptr, nullptr);
  }
  if (!g_peer_connection_factory.get()) {
    DeletePeerConnection();
    return false;
  }

  g_peer_count++;
  if (!CreatePeerConnection(turn_urls, no_of_urls, username, credential)) {
    DeletePeerConnection();
    return false;
  }

  uint32_t maxFileSize = 10000000;
  std::string logFilePath = GetLogFilePath();
  int64_t outputPeriodMs = 16 * 60;  // every 60th frame assuming 16ms frame
                                     // time

  if (!peer_connection_->StartRtcEventLog(
          absl::make_unique<webrtc::RtcEventLogOutputFile>(logFilePath,
                                                           maxFileSize),
          outputPeriodMs))
    RTC_LOG(LS_ERROR) << "Failed to start event log";

  mandatory_receive_ = is_receiver;
  return peer_connection_.get() != nullptr;
}

bool SimplePeerConnection::CreatePeerConnection(const char** turn_urls,
                                                const int no_of_urls,
                                                const char* username,
                                                const char* credential) {
  RTC_DCHECK(g_peer_connection_factory.get() != nullptr);
  RTC_DCHECK(peer_connection_.get() == nullptr);

  local_video_observer_.reset(new VideoObserver());
  remote_video_observer_.reset(new VideoObserver());

  // Add the turn server.
  if (turn_urls != nullptr) {
    if (no_of_urls > 0) {
      webrtc::PeerConnectionInterface::IceServer turn_server;
      for (int i = 0; i < no_of_urls; i++) {
        std::string url(turn_urls[i]);
        if (url.length() > 0)
          turn_server.urls.push_back(turn_urls[i]);
      }

      std::string user_name(username);
      if (user_name.length() > 0)
        turn_server.username = username;

      std::string password(credential);
      if (password.length() > 0)
        turn_server.password = credential;

      config_.servers.push_back(turn_server);
    }
  }

  // Add the stun server.
  webrtc::PeerConnectionInterface::IceServer stun_server;
  stun_server.uri = GetPeerConnectionString();

  // Temp: We shadow the config to figure out the memory corruption bug with the member config
  webrtc::PeerConnectionInterface::RTCConfiguration config_;

  config_.servers.push_back(stun_server);
  config_.enable_dtls_srtp =
      true;  // set this to true otherwise chrome complains
  // TODO: we should migrate to unified plan sdp because that's the standard.
  // more info here:
  // https://docs.google.com/document/d/1PPHWV6108znP1tk_rkCnyagH9FK205hHeE9k5mhUzOg/edit
  config_.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
  config_.set_cpu_adaptation(false);
  config_.set_prerenderer_smoothing(true);

  peer_connection_ = g_peer_connection_factory->CreatePeerConnection(
      config_, nullptr, nullptr, this);

  return peer_connection_.get() != nullptr;
}

void SimplePeerConnection::DeletePeerConnection() {
  g_peer_count--;

#if defined(WEBRTC_ANDROID)
  if (g_camera) {
    JNIEnv* env = webrtc::jni::GetEnv();
    jclass pc_factory_class =
        unity_plugin::FindClass(env, "org/webrtc/UnityUtility");
    jmethodID stop_camera_method = webrtc::GetStaticMethodID(
        env, pc_factory_class, "StopCamera", "(Lorg/webrtc/VideoCapturer;)V");

    env->CallStaticVoidMethod(pc_factory_class, stop_camera_method, g_camera);
    CHECK_EXCEPTION(env);

    g_camera = nullptr;
  }
#endif

  peer_connection_->StopRtcEventLog();
  peer_connection_->GetStats(stats_observer_.get());

  CloseDataChannel();
  peer_connection_ = nullptr;
  active_streams_.clear();

  if (g_peer_count == 0) {
    g_peer_connection_factory = nullptr;
    g_signaling_thread.reset();
    g_worker_thread.reset();
  }
}

bool SimplePeerConnection::CreateOffer() {
  if (!peer_connection_.get())
    return false;

  webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;
  // if (mandatory_receive_) {
  //   options.offer_to_receive_audio = true;
  //   options.offer_to_receive_video = true;
  // }
  peer_connection_->CreateOffer(this, options);

  return true;
}

bool SimplePeerConnection::CreateAnswer() {
  if (!peer_connection_.get())
    return false;

  webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;
  if (mandatory_receive_) {
    options.offer_to_receive_audio = true;
    options.offer_to_receive_video = true;
  }
  peer_connection_->CreateAnswer(this, options);
  return true;
}

void SimplePeerConnection::OnSuccess(
    webrtc::SessionDescriptionInterface* desc) {
  peer_connection_->SetLocalDescription(
      DummySetSessionDescriptionObserver::Create(), desc);

  std::string sdp;
  desc->ToString(&sdp);

  if (OnLocalSdpReady)
    OnLocalSdpReady(desc->type().c_str(), sdp.c_str(),
                    local_sdp_callback_userdata_);
}

void SimplePeerConnection::OnFailure(webrtc::RTCError error) {
  RTC_LOG(LERROR) << ToString(error.type()) << ": " << error.message();

  // TODO(hta): include error.type in the message
  if (OnFailureMessage)
    OnFailureMessage(error.message());
}

void SimplePeerConnection::OnIceCandidate(
    const webrtc::IceCandidateInterface* candidate) {
  RTC_LOG(INFO) << __FUNCTION__ << " " << candidate->sdp_mline_index();

  std::string sdp;
  if (!candidate->ToString(&sdp)) {
    RTC_LOG(LS_ERROR) << "Failed to serialize candidate";
    return;
  }

  if (OnIceCandiateReady)
    OnIceCandiateReady(sdp.c_str(), candidate->sdp_mline_index(),
                       candidate->sdp_mid().c_str(),
                       ice_candidate_send_userdata_);
}

void SimplePeerConnection::RegisterOnLocalI420FrameReady(
    I420FRAMEREADY_CALLBACK callback,
    void* userData) {
  if (local_video_observer_)
    local_video_observer_->SetVideoCallback(callback, userData);
}

void SimplePeerConnection::RegisterOnRemoteI420FrameReady(
    I420FRAMEREADY_CALLBACK callback,
    void* userData) {
  if (remote_video_observer_)
    remote_video_observer_->SetVideoCallback(callback, userData);
}

void SimplePeerConnection::RegisterOnLocalDataChannelReady(
    LOCALDATACHANNELREADY_CALLBACK callback,
    void* userData) {
  OnLocalDataChannelReady = callback;
  local_datachannel_ready_callback_ = userData;
}

void SimplePeerConnection::RegisterOnDataFromDataChannelReady(
    DATAFROMEDATECHANNELREADY_CALLBACK callback,
    void* userData) {
  OnDataFromDataChannelReady = callback;
  on_datachannel_data_ready_userdata_ = userData;
}

void SimplePeerConnection::RegisterOnFailure(FAILURE_CALLBACK callback) {
  OnFailureMessage = callback;
}

void SimplePeerConnection::RegisterOnAudioBusReady(
    AUDIOBUSREADY_CALLBACK callback) {
  OnAudioReady = callback;
}

void SimplePeerConnection::RegisterOnLocalSdpReadytoSend(
    LOCALSDPREADYTOSEND_CALLBACK callback,
    void* userData) {
  OnLocalSdpReady = callback;
  local_sdp_callback_userdata_ = userData;
}

void SimplePeerConnection::RegisterOnIceCandiateReadytoSend(
    ICECANDIDATEREADYTOSEND_CALLBACK callback,
    void* userData) {
  OnIceCandiateReady = callback;
  ice_candidate_send_userdata_ = userData;
}

bool SimplePeerConnection::SetRemoteDescription(const char* type,
                                                const char* sdp) {
  if (!peer_connection_)
    return false;

  std::string remote_desc(sdp);
  std::string sdp_type(type);
  webrtc::SdpParseError error;
  webrtc::SessionDescriptionInterface* session_description(
      webrtc::CreateSessionDescription(sdp_type, remote_desc, &error));
  if (!session_description) {
    RTC_LOG(WARNING) << "Can't parse received session description message. "
                     << "SdpParseError was: " << error.description;
    return false;
  }
  RTC_LOG(INFO) << " Received session description :" << remote_desc;
  peer_connection_->SetRemoteDescription(
      DummySetSessionDescriptionObserver::Create(), session_description);

  return true;
}

bool SimplePeerConnection::AddIceCandidate(const char* candidate,
                                           const int sdp_mlineindex,
                                           const char* sdp_mid) {
  if (!peer_connection_)
    return false;

  webrtc::SdpParseError error;
  std::unique_ptr<webrtc::IceCandidateInterface> ice_candidate(
      webrtc::CreateIceCandidate(sdp_mid, sdp_mlineindex, candidate, &error));
  if (!ice_candidate.get()) {
    RTC_LOG(WARNING) << "Can't parse received candidate message. "
                     << "SdpParseError was: " << error.description;
    return false;
  }
  if (!peer_connection_->AddIceCandidate(ice_candidate.get())) {
    RTC_LOG(WARNING) << "Failed to apply the received candidate";
    return false;
  }
  RTC_LOG(INFO) << " Received candidate :" << candidate;
  return true;
}

void SimplePeerConnection::SetAudioControl(bool is_mute, bool is_record) {
  is_mute_audio_ = is_mute;
  is_record_audio_ = is_record;

  SetAudioControl();
}

void SimplePeerConnection::SetAudioControl() {
  if (!remote_stream_)
    return;
  webrtc::AudioTrackVector tracks = remote_stream_->GetAudioTracks();
  if (tracks.empty())
    return;

  webrtc::AudioTrackInterface* audio_track = tracks[0];
  std::string id = audio_track->id();
  if (is_record_audio_)
    audio_track->AddSink(this);
  else
    audio_track->RemoveSink(this);

  for (auto& track : tracks) {
    if (is_mute_audio_)
      track->set_enabled(false);
    else
      track->set_enabled(true);
  }
}

void SimplePeerConnection::OnAddStream(
    rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) {
  RTC_LOG(INFO) << __FUNCTION__ << " " << stream->id();
  remote_stream_ = stream;
  if (remote_video_observer_ && !remote_stream_->GetVideoTracks().empty()) {
    auto track = remote_stream_->GetVideoTracks()[0];

    track->AddOrUpdateSink(remote_video_observer_.get(), rtc::VideoSinkWants());
  }
  SetAudioControl();
}

std::unique_ptr<cricket::VideoCapturer>
SimplePeerConnection::OpenVideoCaptureDevice() {
  std::vector<std::string> device_names;
  {
    std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo> info(
        webrtc::VideoCaptureFactory::CreateDeviceInfo());
    if (!info) {
      return nullptr;
    }
    int num_devices = info->NumberOfDevices();
    for (int i = 0; i < num_devices; ++i) {
      const uint32_t kSize = 256;
      char name[kSize] = {0};
      char id[kSize] = {0};
      if (info->GetDeviceName(i, name, kSize, id, kSize) != -1) {
        device_names.push_back(name);
      }
    }
  }

  cricket::WebRtcVideoDeviceCapturerFactory factory;
  std::unique_ptr<cricket::VideoCapturer> capturer;
  for (const auto& name : device_names) {
    capturer = factory.Create(cricket::Device(name, 0));
    if (capturer) {
      break;
    }
  }
  return capturer;
}

void SimplePeerConnection::AddStreams(bool audio_only) {
  if (active_streams_.find(kStreamId) != active_streams_.end())
    return;  // Already added.

  rtc::scoped_refptr<webrtc::MediaStreamInterface> stream =
      g_peer_connection_factory->CreateLocalMediaStream(kStreamId);

  rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track(
      g_peer_connection_factory->CreateAudioTrack(
          kAudioLabel, g_peer_connection_factory->CreateAudioSource(
                           cricket::AudioOptions())));
  std::string id = audio_track->id();

  peer_connection_->AddTrack(audio_track, {kStreamId})
      .value();  // TODO: handle case where this fails. In debug mode it
                 // asserts.

  if (!audio_only) {
#if defined(WEBRTC_ANDROID)
    JNIEnv* env = webrtc::jni::GetEnv();
    jclass pc_factory_class =
        unity_plugin::FindClass(env, "org/webrtc/UnityUtility");
    jmethodID load_texture_helper_method = webrtc::GetStaticMethodID(
        env, pc_factory_class, "LoadSurfaceTextureHelper",
        "()Lorg/webrtc/SurfaceTextureHelper;");
    jobject texture_helper = env->CallStaticObjectMethod(
        pc_factory_class, load_texture_helper_method);
    CHECK_EXCEPTION(env);
    RTC_DCHECK(texture_helper != nullptr)
        << "Cannot get the Surface Texture Helper.";

    rtc::scoped_refptr<webrtc::jni::AndroidVideoTrackSource> source(
        new rtc::RefCountedObject<webrtc::jni::AndroidVideoTrackSource>(
            g_signaling_thread.get(), env, false));
    rtc::scoped_refptr<webrtc::VideoTrackSourceProxy> proxy_source =
        webrtc::VideoTrackSourceProxy::Create(g_signaling_thread.get(),
                                              g_worker_thread.get(), source);

    // link with VideoCapturer (Camera);
    jmethodID link_camera_method = webrtc::GetStaticMethodID(
        env, pc_factory_class, "LinkCamera",
        "(JLorg/webrtc/SurfaceTextureHelper;)Lorg/webrtc/VideoCapturer;");
    jobject camera_tmp =
        env->CallStaticObjectMethod(pc_factory_class, link_camera_method,
                                    (jlong)proxy_source.get(), texture_helper);
    CHECK_EXCEPTION(env);
    g_camera = (jobject)env->NewGlobalRef(camera_tmp);

    rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track(
        g_peer_connection_factory->CreateVideoTrack(kVideoLabel,
                                                    proxy_source.release()));
    stream->AddTrack(video_track);
#else

    auto d3d_source = hololight::D3D11VideoFrameSource::Create(
        d3d_device_.get(), d3d_context_.get(), &d3d_render_target_desc_,
        g_signaling_thread.get());

    rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track(
          g_peer_connection_factory->CreateVideoTrack(
              kVideoLabel, d3d_source /*g_peer_connection_factory->CreateVideoSource(
                               std::move(capture), nullptr)*/));
    local_d3d_track_source_ = d3d_source;

    // webrtc::RtpTransceiverInit init_options;
    // init_options.direction = webrtc::RtpTransceiverDirection::kSendOnly;
    // //server should only send in the future. what if we don't need a
    // transceiver, only tracks?
    auto track_result = peer_connection_->AddTrack(video_track, {kStreamId});
    if (!track_result.ok())
      RTC_LOG(LS_ERROR) << "AddTrack failed";

    auto track = track_result.value();

    auto track_params = track->GetParameters();
    track_params.degradation_preference =
        webrtc::DegradationPreference::DISABLED;
    track->SetParameters(track_params);

    video_track->AddOrUpdateSink(local_video_observer_.get(),
                                 rtc::VideoSinkWants());
#endif
    if (local_video_observer_ && !stream->GetVideoTracks().empty()) {
      stream->GetVideoTracks()[0]->AddOrUpdateSink(local_video_observer_.get(),
                                                   rtc::VideoSinkWants());
    }
  }

  // I don't think we need this any more, or rather we should use a different
  // way of keeping track of streams/tracks.
  typedef std::pair<std::string,
                    rtc::scoped_refptr<webrtc::MediaStreamInterface>>
      MediaStreamPair;
  active_streams_.insert(MediaStreamPair(stream->id(), stream));
}

void SimplePeerConnection::OnD3DFrame(ID3D11Texture2D* rendered_image) {
  local_d3d_track_source_->OnFrameCaptured(rendered_image);
}

bool SimplePeerConnection::CreateDataChannel() {
  struct webrtc::DataChannelInit init;
  init.ordered = true;   // TODO: Make configurable
  init.reliable = true;  // TODO: Make configurable

  // // These settings enable out-of-band datachannel synchronization, making
  // them available sooner
  // // Some info can be found here: https://github.com/w3c/ortc/issues/233
  // init.negotiated = true;
  // init.id = 0;
  init.negotiated = false;  // For now, we use in-band signalling of this thing.
                            // TODO: CHANGE THIS

  data_channel_ = peer_connection_->CreateDataChannel("Hello", &init);
  if (data_channel_.get()) {
    data_channel_->RegisterObserver(this);
    RTC_LOG(LS_INFO) << "Succeeds to create data channel";
    return true;
  } else {
    RTC_LOG(LS_INFO) << "Fails to create data channel";
    return false;
  }
}

void SimplePeerConnection::CloseDataChannel() {
  if (data_channel_.get()) {
    data_channel_->UnregisterObserver();
    data_channel_->Close();
  }
  data_channel_ = nullptr;
}

bool SimplePeerConnection::SendDataViaDataChannel(const std::string& data) {
  if (!data_channel_.get()) {
    RTC_LOG(LS_INFO) << "Data channel is not established";
    return false;
  }
  webrtc::DataBuffer buffer(data);
  return data_channel_->Send(buffer);
  // return true; TODO: Jesus google learn to return your booleans
}

// Peerconnection observer
void SimplePeerConnection::OnDataChannel(
    rtc::scoped_refptr<webrtc::DataChannelInterface> channel) {
  data_channel_ = channel;
  channel->RegisterObserver(this);
}

void SimplePeerConnection::OnStateChange() {
  RTC_LOG(LS_INFO) << "Data channel state changed";

  if (data_channel_) {
    webrtc::DataChannelInterface::DataState state = data_channel_->state();
    if (state == webrtc::DataChannelInterface::kOpen) {
      if (OnLocalDataChannelReady)
        OnLocalDataChannelReady(local_datachannel_ready_callback_);
      RTC_LOG(LS_INFO) << "Data channel is open";
    }
  }
}

//  A data buffer was successfully received.
void SimplePeerConnection::OnMessage(const webrtc::DataBuffer& buffer) {
  RTC_LOG(LS_INFO) << "Received data from data channel";

  size_t size = buffer.data.size();
  char* msg = new char[size + 1];
  memcpy(msg, buffer.data.data(), size);
  msg[size] = 0;
  if (OnDataFromDataChannelReady)
    OnDataFromDataChannelReady(msg, on_datachannel_data_ready_userdata_);
  delete[] msg;
}

// AudioTrackSinkInterface implementation.
void SimplePeerConnection::OnData(const void* audio_data,
                                  int bits_per_sample,
                                  int sample_rate,
                                  size_t number_of_channels,
                                  size_t number_of_frames) {
  if (OnAudioReady)
    OnAudioReady(audio_data, bits_per_sample, sample_rate,
                 static_cast<int>(number_of_channels),
                 static_cast<int>(number_of_frames));
}

std::vector<uint32_t> SimplePeerConnection::GetRemoteAudioTrackSsrcs() {
  std::vector<rtc::scoped_refptr<webrtc::RtpReceiverInterface>> receivers =
      peer_connection_->GetReceivers();

  std::vector<uint32_t> ssrcs;
  for (const auto& receiver : receivers) {
    if (receiver->media_type() != cricket::MEDIA_TYPE_AUDIO)
      continue;

    std::vector<webrtc::RtpEncodingParameters> params =
        receiver->GetParameters().encodings;

    for (const auto& param : params) {
      uint32_t ssrc = param.ssrc.value_or(0);
      if (ssrc > 0)
        ssrcs.push_back(ssrc);
    }
  }

  return ssrcs;
}
