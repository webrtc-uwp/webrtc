/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/pc/peerconnectionobserver_jni.h"

#include <pc/mediastreamobserver.h>
#include <string>

#include "sdk/android/src/jni/classreferenceholder.h"
#include "sdk/android/src/jni/pc/java_native_conversion.h"

namespace webrtc {
namespace jni {

// Convenience, used since callbacks occur on the signaling thread, which may
// be a non-Java thread.
static JNIEnv* jni() {
  return AttachCurrentThreadIfNeeded();
}

PeerConnectionObserverJni::PeerConnectionObserverJni(JNIEnv* jni,
                                                     jobject j_observer)
    : j_observer_global_(jni, j_observer),
      j_observer_class_(jni, GetObjectClass(jni, *j_observer_global_)),
      j_media_stream_class_(jni, FindClass(jni, "org/webrtc/MediaStream")),
      j_media_stream_ctor_(
          GetMethodID(jni, *j_media_stream_class_, "<init>", "(J)V")),
      j_audio_track_class_(jni, FindClass(jni, "org/webrtc/AudioTrack")),
      j_audio_track_ctor_(
          GetMethodID(jni, *j_audio_track_class_, "<init>", "(J)V")),
      j_video_track_class_(jni, FindClass(jni, "org/webrtc/VideoTrack")),
      j_video_track_ctor_(
          GetMethodID(jni, *j_video_track_class_, "<init>", "(J)V")),
      j_data_channel_class_(jni, FindClass(jni, "org/webrtc/DataChannel")),
      j_data_channel_ctor_(
          GetMethodID(jni, *j_data_channel_class_, "<init>", "(J)V")),
      j_rtp_receiver_class_(jni, FindClass(jni, "org/webrtc/RtpReceiver")),
      j_rtp_receiver_ctor_(
          GetMethodID(jni, *j_rtp_receiver_class_, "<init>", "(J)V")) {}

PeerConnectionObserverJni::~PeerConnectionObserverJni() {
  ScopedLocalRefFrame local_ref_frame(jni());
  stream_observers_.clear();
  while (!remote_tracks_.empty()) {
    NativeToJavaMediaTrackMap::iterator it = remote_tracks_.begin();
    DeleteGlobalRef(jni(), it->second);
    remote_tracks_.erase(it);
  }
  while (!remote_streams_.empty())
    DisposeRemoteStream(remote_streams_.begin());
  while (!rtp_receivers_.empty())
    DisposeRtpReceiver(rtp_receivers_.begin());
}

void PeerConnectionObserverJni::OnIceCandidate(
    const IceCandidateInterface* candidate) {
  ScopedLocalRefFrame local_ref_frame(jni());
  std::string sdp;
  RTC_CHECK(candidate->ToString(&sdp)) << "got so far: " << sdp;
  jclass candidate_class = FindClass(jni(), "org/webrtc/IceCandidate");
  jmethodID ctor =
      GetMethodID(jni(), candidate_class, "<init>",
                  "(Ljava/lang/String;ILjava/lang/String;Ljava/lang/String;)V");
  jstring j_mid = JavaStringFromStdString(jni(), candidate->sdp_mid());
  jstring j_sdp = JavaStringFromStdString(jni(), sdp);
  jstring j_url = JavaStringFromStdString(jni(), candidate->candidate().url());
  jobject j_candidate = jni()->NewObject(
      candidate_class, ctor, j_mid, candidate->sdp_mline_index(), j_sdp, j_url);
  CHECK_EXCEPTION(jni()) << "error during NewObject";
  jmethodID m = GetMethodID(jni(), *j_observer_class_, "onIceCandidate",
                            "(Lorg/webrtc/IceCandidate;)V");
  jni()->CallVoidMethod(*j_observer_global_, m, j_candidate);
  CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";
}

void PeerConnectionObserverJni::OnIceCandidatesRemoved(
    const std::vector<cricket::Candidate>& candidates) {
  ScopedLocalRefFrame local_ref_frame(jni());
  jobjectArray candidates_array = NativeToJavaCandidateArray(jni(), candidates);
  jmethodID m = GetMethodID(jni(), *j_observer_class_, "onIceCandidatesRemoved",
                            "([Lorg/webrtc/IceCandidate;)V");
  jni()->CallVoidMethod(*j_observer_global_, m, candidates_array);
  CHECK_EXCEPTION(jni()) << "Error during CallVoidMethod";
}

void PeerConnectionObserverJni::OnSignalingChange(
    PeerConnectionInterface::SignalingState new_state) {
  ScopedLocalRefFrame local_ref_frame(jni());
  jmethodID m = GetMethodID(jni(), *j_observer_class_, "onSignalingChange",
                            "(Lorg/webrtc/PeerConnection$SignalingState;)V");
  jobject new_state_enum = JavaEnumFromIndexAndClassName(
      jni(), "PeerConnection$SignalingState", new_state);
  jni()->CallVoidMethod(*j_observer_global_, m, new_state_enum);
  CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";
}

void PeerConnectionObserverJni::OnIceConnectionChange(
    PeerConnectionInterface::IceConnectionState new_state) {
  ScopedLocalRefFrame local_ref_frame(jni());
  jmethodID m =
      GetMethodID(jni(), *j_observer_class_, "onIceConnectionChange",
                  "(Lorg/webrtc/PeerConnection$IceConnectionState;)V");
  jobject new_state_enum = JavaEnumFromIndexAndClassName(
      jni(), "PeerConnection$IceConnectionState", new_state);
  jni()->CallVoidMethod(*j_observer_global_, m, new_state_enum);
  CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";
}

void PeerConnectionObserverJni::OnIceConnectionReceivingChange(bool receiving) {
  ScopedLocalRefFrame local_ref_frame(jni());
  jmethodID m = GetMethodID(jni(), *j_observer_class_,
                            "onIceConnectionReceivingChange", "(Z)V");
  jni()->CallVoidMethod(*j_observer_global_, m, receiving);
  CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";
}

void PeerConnectionObserverJni::OnIceGatheringChange(
    PeerConnectionInterface::IceGatheringState new_state) {
  ScopedLocalRefFrame local_ref_frame(jni());
  jmethodID m = GetMethodID(jni(), *j_observer_class_, "onIceGatheringChange",
                            "(Lorg/webrtc/PeerConnection$IceGatheringState;)V");
  jobject new_state_enum = JavaEnumFromIndexAndClassName(
      jni(), "PeerConnection$IceGatheringState", new_state);
  jni()->CallVoidMethod(*j_observer_global_, m, new_state_enum);
  CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";
}

void PeerConnectionObserverJni::OnAddStream(
    rtc::scoped_refptr<MediaStreamInterface> stream) {
  ScopedLocalRefFrame local_ref_frame(jni());
  // The stream could be added into the remote_streams_ map when calling
  // OnAddTrack.
  jobject j_stream = GetOrCreateJavaStream(stream);

  jmethodID m = GetMethodID(jni(), *j_observer_class_, "onAddStream",
                            "(Lorg/webrtc/MediaStream;)V");
  jni()->CallVoidMethod(*j_observer_global_, m, j_stream);
  CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";

  MediaStreamObserver* observer = new MediaStreamObserver(stream);
  observer->SignalAudioTrackRemoved.connect(
      this, &PeerConnectionObserverJni::OnAudioTrackRemoved);
  observer->SignalVideoTrackRemoved.connect(
      this, &PeerConnectionObserverJni::OnVideoTrackRemoved);
  stream_observers_.push_back(std::unique_ptr<MediaStreamObserver>(observer));
}

void PeerConnectionObserverJni::OnAudioTrackAdded(
    AudioTrackInterface* track,
    MediaStreamInterface* stream) {
  ScopedLocalRefFrame local_ref_frame(jni());
  jstring id = JavaStringFromStdString(jni(), track->id());
  jobject j_stream = GetOrCreateJavaStream(stream);
  // Java AudioTrack holds one reference. Corresponding Release() is in
  // MediaStreamTrack_free, triggered by AudioTrack.dispose().
  track->AddRef();
  jobject j_track = jni()->NewObject(*j_audio_track_class_, j_audio_track_ctor_,
                                     reinterpret_cast<jlong>(track), id);
  CHECK_EXCEPTION(jni()) << "error during NewObject";
  remote_tracks_[track] = NewGlobalRef(jni(), j_track);
  jfieldID audio_tracks_id = GetFieldID(
      jni(), *j_media_stream_class_, "audioTracks", "Ljava/util/LinkedList;");
  jobject audio_tracks = GetObjectField(jni(), j_stream, audio_tracks_id);
  jmethodID add = GetMethodID(jni(), GetObjectClass(jni(), audio_tracks), "add",
                              "(Ljava/lang/Object;)Z");
  jboolean added = jni()->CallBooleanMethod(audio_tracks, add, j_track);
  CHECK_EXCEPTION(jni()) << "error during CallBooleanMethod";
  RTC_CHECK(added);
}

void PeerConnectionObserverJni::OnVideoTrackAdded(
    VideoTrackInterface* track,
    MediaStreamInterface* stream) {
  ScopedLocalRefFrame local_ref_frame(jni());
  jobject j_stream = GetOrCreateJavaStream(stream);

  jstring id = JavaStringFromStdString(jni(), track->id());
  // Java VideoTrack holds one reference. Corresponding Release() is in
  // MediaStreamTrack_free, triggered by VideoTrack.dispose().
  track->AddRef();
  jobject j_track = jni()->NewObject(*j_video_track_class_, j_video_track_ctor_,
                                     reinterpret_cast<jlong>(track), id);
  CHECK_EXCEPTION(jni()) << "error during NewObject";
  remote_tracks_[track] = NewGlobalRef(jni(), j_track);
  jfieldID video_tracks_id = GetFieldID(
      jni(), *j_media_stream_class_, "videoTracks", "Ljava/util/LinkedList;");
  jobject video_tracks = GetObjectField(jni(), j_stream, video_tracks_id);
  jmethodID add = GetMethodID(jni(), GetObjectClass(jni(), video_tracks), "add",
                              "(Ljava/lang/Object;)Z");
  jboolean added = jni()->CallBooleanMethod(video_tracks, add, j_track);
  CHECK_EXCEPTION(jni()) << "error during CallBooleanMethod";
  RTC_CHECK(added);
}

void PeerConnectionObserverJni::OnAudioTrackRemoved(
    AudioTrackInterface* track,
    MediaStreamInterface* stream) {
  ScopedLocalRefFrame local_ref_frame(jni());
  jobject j_stream = GetOrCreateJavaStream(stream);
  NativeToJavaMediaTrackMap::iterator track_it = remote_tracks_.find(track);
  RTC_CHECK(track_it != remote_tracks_.end());
  NativeMediaStreamTrackToNativeRtpReceiver::iterator receiver_it =
      track_to_receiver_.find(track);
  RTC_CHECK(receiver_it != track_to_receiver_.end());
  NativeToJavaRtpReceiverMap::iterator j_receiver_it =
      rtp_receivers_.find(receiver_it->second);
  RTC_CHECK(j_receiver_it != rtp_receivers_.end());

  jobject j_track = track_it->second;
  jobject j_receiver = j_receiver_it->second;

  jfieldID audio_tracks_id = GetFieldID(
      jni(), *j_media_stream_class_, "audioTracks", "Ljava/util/LinkedList;");
  jobject video_tracks = GetObjectField(jni(), j_stream, audio_tracks_id);
  jmethodID remove = GetMethodID(jni(), GetObjectClass(jni(), video_tracks),
                                 "remove", "(Ljava/lang/Object;)Z");
  jboolean removed = jni()->CallBooleanMethod(video_tracks, remove, j_track);
  CHECK_EXCEPTION(jni()) << "error during CallBooleanMethod";
  RTC_CHECK(removed);

  std::vector<rtc::scoped_refptr<MediaStreamInterface>> streams;
  streams.push_back(rtc::scoped_refptr<MediaStreamInterface>(stream));
  jobjectArray j_stream_array = NativeToJavaMediaStreamArray(jni(), streams);
  jmethodID m =
      GetMethodID(jni(), *j_observer_class_, "onRemoveTrack",
                  "(Lorg/webrtc/RtpReceiver;[Lorg/webrtc/MediaStream;)V");
  jni()->CallVoidMethod(*j_observer_global_, m, j_receiver, j_stream_array);
  CHECK_EXCEPTION(jni()) << "Error during CallVoidMethod";

  DisposeRemoteTrack(track_it);
  DisposeRtpReceiver(j_receiver_it);
}

void PeerConnectionObserverJni::OnVideoTrackRemoved(
    VideoTrackInterface* track,
    MediaStreamInterface* stream) {
  ScopedLocalRefFrame local_ref_frame(jni());
  jobject j_stream = GetOrCreateJavaStream(stream);
  NativeToJavaMediaTrackMap::iterator track_it = remote_tracks_.find(track);
  RTC_CHECK(track_it != remote_tracks_.end());
  NativeMediaStreamTrackToNativeRtpReceiver::iterator receiver_it =
      track_to_receiver_.find(track);
  RTC_CHECK(receiver_it != track_to_receiver_.end());
  NativeToJavaRtpReceiverMap::iterator j_receiver_it =
      rtp_receivers_.find(receiver_it->second);
  RTC_CHECK(j_receiver_it != rtp_receivers_.end());

  jobject j_track = track_it->second;
  jobject j_receiver = j_receiver_it->second;

  jfieldID video_tracks_id = GetFieldID(
      jni(), *j_media_stream_class_, "videoTracks", "Ljava/util/LinkedList;");
  jobject video_tracks = GetObjectField(jni(), j_stream, video_tracks_id);
  jmethodID remove = GetMethodID(jni(), GetObjectClass(jni(), video_tracks),
                                 "remove", "(Ljava/lang/Object;)Z");
  jboolean removed = jni()->CallBooleanMethod(video_tracks, remove, j_track);
  CHECK_EXCEPTION(jni()) << "error during CallBooleanMethod";
  RTC_CHECK(removed);

  std::vector<rtc::scoped_refptr<MediaStreamInterface>> streams;
  streams.push_back(rtc::scoped_refptr<MediaStreamInterface>(stream));
  jobjectArray j_stream_array = NativeToJavaMediaStreamArray(jni(), streams);
  jmethodID m =
      GetMethodID(jni(), *j_observer_class_, "onRemoveTrack",
                  "(Lorg/webrtc/RtpReceiver;[Lorg/webrtc/MediaStream;)V");
  jni()->CallVoidMethod(*j_observer_global_, m, j_receiver, j_stream_array);
  CHECK_EXCEPTION(jni()) << "Error during CallVoidMethod";

  DisposeRemoteTrack(track_it);
  DisposeRtpReceiver(j_receiver_it);
}

void PeerConnectionObserverJni::OnRemoveStream(
    rtc::scoped_refptr<MediaStreamInterface> stream) {
  ScopedLocalRefFrame local_ref_frame(jni());
  NativeToJavaStreamsMap::iterator it = remote_streams_.find(stream);
  RTC_CHECK(it != remote_streams_.end())
      << "unexpected stream: " << std::hex << stream;
  jobject j_stream = it->second;
  jmethodID m = GetMethodID(jni(), *j_observer_class_, "onRemoveStream",
                            "(Lorg/webrtc/MediaStream;)V");
  jni()->CallVoidMethod(*j_observer_global_, m, j_stream);
  CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";

  stream_observers_.erase(
      std::remove_if(
          stream_observers_.begin(), stream_observers_.end(),
          [stream](const std::unique_ptr<MediaStreamObserver>& observer) {
            return observer->stream() == stream;
          }),
      stream_observers_.end());

  // Release the refptr reference so that DisposeRemoteStream can assert
  // it removes the final reference.
  stream = nullptr;
  DisposeRemoteStream(it);
}

void PeerConnectionObserverJni::OnDataChannel(
    rtc::scoped_refptr<DataChannelInterface> channel) {
  ScopedLocalRefFrame local_ref_frame(jni());
  jobject j_channel =
      jni()->NewObject(*j_data_channel_class_, j_data_channel_ctor_,
                       jlongFromPointer(channel.get()));
  CHECK_EXCEPTION(jni()) << "error during NewObject";

  jmethodID m = GetMethodID(jni(), *j_observer_class_, "onDataChannel",
                            "(Lorg/webrtc/DataChannel;)V");
  jni()->CallVoidMethod(*j_observer_global_, m, j_channel);

  // Channel is now owned by Java object, and will be freed from
  // DataChannel.dispose().  Important that this be done _after_ the
  // CallVoidMethod above as Java code might call back into native code and be
  // surprised to see a refcount of 2.
  int bumped_count = channel->AddRef();
  RTC_CHECK(bumped_count == 2) << "Unexpected refcount OnDataChannel";

  CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";
}

void PeerConnectionObserverJni::OnRenegotiationNeeded() {
  ScopedLocalRefFrame local_ref_frame(jni());
  jmethodID m =
      GetMethodID(jni(), *j_observer_class_, "onRenegotiationNeeded", "()V");
  jni()->CallVoidMethod(*j_observer_global_, m);
  CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";
}

void PeerConnectionObserverJni::OnAddTrack(
    rtc::scoped_refptr<RtpReceiverInterface> receiver,
    const std::vector<rtc::scoped_refptr<MediaStreamInterface>>& streams) {
  ScopedLocalRefFrame local_ref_frame(jni());
  jobject j_rtp_receiver =
      jni()->NewObject(*j_rtp_receiver_class_, j_rtp_receiver_ctor_,
                       jlongFromPointer(receiver.get()));
  CHECK_EXCEPTION(jni()) << "error during NewObject";
  receiver->AddRef();
  rtp_receivers_[receiver] = NewGlobalRef(jni(), j_rtp_receiver);
  track_to_receiver_[receiver->track()] = receiver;
  for (auto stream : streams) {
    if (receiver->media_type() == cricket::MediaType::MEDIA_TYPE_AUDIO) {
      AudioTrackInterface* track =
          reinterpret_cast<AudioTrackInterface*>(receiver->track().get());
      OnAudioTrackAdded(track, stream);
    } else if (receiver->media_type() == cricket::MediaType::MEDIA_TYPE_VIDEO) {
      VideoTrackInterface* track =
          reinterpret_cast<VideoTrackInterface*>(receiver->track().get());
      OnVideoTrackAdded(track, stream);
    } else {
      RTC_NOTREACHED();
    }
  }

  jobjectArray j_stream_array = NativeToJavaMediaStreamArray(jni(), streams);
  jmethodID m =
      GetMethodID(jni(), *j_observer_class_, "onAddTrack",
                  "(Lorg/webrtc/RtpReceiver;[Lorg/webrtc/MediaStream;)V");
  jni()->CallVoidMethod(*j_observer_global_, m, j_rtp_receiver, j_stream_array);
  CHECK_EXCEPTION(jni()) << "Error during CallVoidMethod";
}

void PeerConnectionObserverJni::SetConstraints(
    MediaConstraintsJni* constraints) {
  RTC_CHECK(!constraints_.get()) << "constraints already set!";
  constraints_.reset(constraints);
}

void PeerConnectionObserverJni::DisposeRemoteStream(
    const NativeToJavaStreamsMap::iterator& it) {
  jobject j_stream = it->second;
  remote_streams_.erase(it);
  jni()->CallVoidMethod(
      j_stream, GetMethodID(jni(), *j_media_stream_class_, "dispose", "()V"));
  CHECK_EXCEPTION(jni()) << "error during MediaStream.dispose()";
  DeleteGlobalRef(jni(), j_stream);
}

void PeerConnectionObserverJni::DisposeRtpReceiver(
    const NativeToJavaRtpReceiverMap::iterator& it) {
  jobject j_rtp_receiver = it->second;
  rtp_receivers_.erase(it);
  jni()->CallVoidMethod(
      j_rtp_receiver,
      GetMethodID(jni(), *j_rtp_receiver_class_, "dispose", "()V"));
  CHECK_EXCEPTION(jni()) << "error during RtpReceiver.dispose()";
  DeleteGlobalRef(jni(), j_rtp_receiver);
}

void PeerConnectionObserverJni::DisposeRemoteTrack(
    const NativeToJavaMediaTrackMap::iterator& it) {
  RTC_CHECK(it != remote_tracks_.end());
  MediaStreamTrackInterface* track = it->first;
  jobject j_track = it->second;
  remote_tracks_.erase(it);
  jmethodID dispose;
  if (track->kind() == MediaStreamTrackInterface::kVideoKind) {
    dispose = GetMethodID(jni(), *j_video_track_class_, "dispose", "()V");
  } else {
    dispose = GetMethodID(jni(), *j_audio_track_class_, "dispose", "()V");
  }
  jni()->CallVoidMethod(j_track, dispose);
  CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";
  DeleteGlobalRef(jni(), j_track);
}

// If the NativeToJavaStreamsMap contains the stream, return it.
// Otherwise, create a new Java MediaStream.
jobject PeerConnectionObserverJni::GetOrCreateJavaStream(
    const rtc::scoped_refptr<MediaStreamInterface>& stream) {
  NativeToJavaStreamsMap::iterator it = remote_streams_.find(stream);
  if (it != remote_streams_.end()) {
    return it->second;
  }
  // Java MediaStream holds one reference. Corresponding Release() is in
  // MediaStream_free, triggered by MediaStream.dispose().
  stream->AddRef();
  jobject j_stream =
      jni()->NewObject(*j_media_stream_class_, j_media_stream_ctor_,
                       reinterpret_cast<jlong>(stream.get()));
  CHECK_EXCEPTION(jni()) << "error during NewObject";

  remote_streams_[stream] = NewGlobalRef(jni(), j_stream);
  return j_stream;
}

jobjectArray PeerConnectionObserverJni::NativeToJavaMediaStreamArray(
    JNIEnv* jni,
    const std::vector<rtc::scoped_refptr<MediaStreamInterface>>& streams) {
  jobjectArray java_streams =
      jni->NewObjectArray(streams.size(), *j_media_stream_class_, nullptr);
  CHECK_EXCEPTION(jni) << "error during NewObjectArray";
  for (size_t i = 0; i < streams.size(); ++i) {
    jobject j_stream = GetOrCreateJavaStream(streams[i]);
    jni->SetObjectArrayElement(java_streams, i, j_stream);
  }
  return java_streams;
}

}  // namespace jni
}  // namespace webrtc
