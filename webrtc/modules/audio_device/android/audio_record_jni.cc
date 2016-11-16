/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_device/android/audio_record_jni.h"

#include <utility>

#include <android/log.h>

#include "webrtc/base/arraysize.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/format_macros.h"
#include "webrtc/modules/audio_device/android/audio_common.h"

#define TAG "AudioRecordJni"
#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, TAG, __VA_ARGS__)
#define ALOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define ALOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)

namespace webrtc {

// AudioRecordJni::JavaAudioRecord implementation.
AudioRecordJni::JavaAudioRecord::JavaAudioRecord(
    NativeRegistration* native_reg,
    std::unique_ptr<GlobalRef> audio_record)
    : audio_record_(std::move(audio_record)),
      init_recording_(native_reg->GetMethodId("initRecording", "(II)I")),
      start_recording_(native_reg->GetMethodId("startRecording", "()Z")),
      stop_recording_(native_reg->GetMethodId("stopRecording", "()Z")),
      enable_built_in_aec_(native_reg->GetMethodId("enableBuiltInAEC", "(Z)Z")),
      enable_built_in_ns_(native_reg->GetMethodId("enableBuiltInNS", "(Z)Z")) {}

AudioRecordJni::JavaAudioRecord::~JavaAudioRecord() {}

int AudioRecordJni::JavaAudioRecord::InitRecording(
    int sample_rate, size_t channels) {
  return audio_record_->CallIntMethod(init_recording_,
                                      static_cast<jint>(sample_rate),
                                      static_cast<jint>(channels));
}

bool AudioRecordJni::JavaAudioRecord::StartRecording() {
  return audio_record_->CallBooleanMethod(start_recording_);
}

bool AudioRecordJni::JavaAudioRecord::StopRecording() {
  return audio_record_->CallBooleanMethod(stop_recording_);
}

bool AudioRecordJni::JavaAudioRecord::EnableBuiltInAEC(bool enable) {
  return audio_record_->CallBooleanMethod(enable_built_in_aec_,
                                          static_cast<jboolean>(enable));
}

bool AudioRecordJni::JavaAudioRecord::EnableBuiltInNS(bool enable) {
  return audio_record_->CallBooleanMethod(enable_built_in_ns_,
                                          static_cast<jboolean>(enable));
}

// AudioRecordJni implementation.
AudioRecordJni::AudioRecordJni(AudioManager* audio_manager)
    : j_environment_(JVM::GetInstance()->environment()),
      audio_manager_(audio_manager),
      audio_parameters_(audio_manager->GetRecordAudioParameters()),
      total_delay_in_milliseconds_(0),
      direct_buffer_address_(nullptr),
      direct_buffer_capacity_in_bytes_(0),
      frames_per_buffer_(0),
      initialized_(false),
      recording_(false),
      audio_device_buffer_(nullptr) {
  ALOGD("ctor%s", GetThreadInfo().c_str());
  RTC_DCHECK(audio_parameters_.is_valid());
  RTC_CHECK(j_environment_);
  JNINativeMethod native_methods[] = {
      {"nativeCacheDirectBufferAddress", "(Ljava/nio/ByteBuffer;J)V",
      reinterpret_cast<void*>(
          &webrtc::AudioRecordJni::CacheDirectBufferAddress)},
      {"nativeDataIsRecorded", "(IJ)V",
      reinterpret_cast<void*>(&webrtc::AudioRecordJni::DataIsRecorded)}};
  j_native_registration_ = j_environment_->RegisterNatives(
      "org/webrtc/voiceengine/WebRtcAudioRecord", native_methods,
      arraysize(native_methods));
  j_audio_record_.reset(new JavaAudioRecord(
      j_native_registration_.get(),
      j_native_registration_->NewObject(
          "<init>", "(Landroid/content/Context;J)V",
          JVM::GetInstance()->context(), PointerTojlong(this))));
  // Detach from this thread since we want to use the checker to verify calls
  // from the Java based audio thread.
  thread_checker_java_.DetachFromThread();
}

AudioRecordJni::~AudioRecordJni() {
  ALOGD("~dtor%s", GetThreadInfo().c_str());
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  Terminate();
}

int32_t AudioRecordJni::Init() {
  ALOGD("Init%s", GetThreadInfo().c_str());
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  return 0;
}

int32_t AudioRecordJni::Terminate() {
  ALOGD("Terminate%s", GetThreadInfo().c_str());
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  StopRecording();
  return 0;
}

int32_t AudioRecordJni::InitRecording() {
  ALOGD("InitRecording%s", GetThreadInfo().c_str());
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  RTC_DCHECK(!initialized_);
  RTC_DCHECK(!recording_);
  int frames_per_buffer = j_audio_record_->InitRecording(
      audio_parameters_.sample_rate(), audio_parameters_.channels());
  if (frames_per_buffer < 0) {
    ALOGE("InitRecording failed!");
    return -1;
  }
  frames_per_buffer_ = static_cast<size_t>(frames_per_buffer);
  ALOGD("frames_per_buffer: %" PRIuS, frames_per_buffer_);
  const size_t bytes_per_frame = audio_parameters_.channels() * sizeof(int16_t);
  RTC_CHECK_EQ(direct_buffer_capacity_in_bytes_,
               frames_per_buffer_ * bytes_per_frame);
  RTC_CHECK_EQ(frames_per_buffer_, audio_parameters_.frames_per_10ms_buffer());
  initialized_ = true;
  return 0;
}

int32_t AudioRecordJni::StartRecording() {
  ALOGD("StartRecording%s", GetThreadInfo().c_str());
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  RTC_DCHECK(initialized_);
  RTC_DCHECK(!recording_);
  if (!j_audio_record_->StartRecording()) {
    ALOGE("StartRecording failed!");
    return -1;
  }
  recording_ = true;
  return 0;
}

int32_t AudioRecordJni::StopRecording() {
  ALOGD("StopRecording%s", GetThreadInfo().c_str());
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  if (!initialized_ || !recording_) {
    return 0;
  }
  if (!j_audio_record_->StopRecording()) {
    ALOGE("StopRecording failed!");
    return -1;
  }
  // If we don't detach here, we will hit a RTC_DCHECK in OnDataIsRecorded()
  // next time StartRecording() is called since it will create a new Java
  // thread.
  thread_checker_java_.DetachFromThread();
  initialized_ = false;
  recording_ = false;
  direct_buffer_address_= nullptr;
  return 0;
}

void AudioRecordJni::AttachAudioBuffer(AudioDeviceBuffer* audioBuffer) {
  ALOGD("AttachAudioBuffer");
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  audio_device_buffer_ = audioBuffer;
  const int sample_rate_hz = audio_parameters_.sample_rate();
  ALOGD("SetRecordingSampleRate(%d)", sample_rate_hz);
  audio_device_buffer_->SetRecordingSampleRate(sample_rate_hz);
  const size_t channels = audio_parameters_.channels();
  ALOGD("SetRecordingChannels(%" PRIuS ")", channels);
  audio_device_buffer_->SetRecordingChannels(channels);
  total_delay_in_milliseconds_ =
      audio_manager_->GetDelayEstimateInMilliseconds();
  RTC_DCHECK_GT(total_delay_in_milliseconds_, 0);
  ALOGD("total_delay_in_milliseconds: %d", total_delay_in_milliseconds_);
}

int32_t AudioRecordJni::EnableBuiltInAEC(bool enable) {
  ALOGD("EnableBuiltInAEC%s", GetThreadInfo().c_str());
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  return j_audio_record_->EnableBuiltInAEC(enable) ? 0 : -1;
}

int32_t AudioRecordJni::EnableBuiltInAGC(bool enable) {
  // TODO(henrika): possibly remove when no longer used by any client.
  FATAL() << "Should never be called";
  return -1;
}

int32_t AudioRecordJni::EnableBuiltInNS(bool enable) {
  ALOGD("EnableBuiltInNS%s", GetThreadInfo().c_str());
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  return j_audio_record_->EnableBuiltInNS(enable) ? 0 : -1;
}

void JNICALL AudioRecordJni::CacheDirectBufferAddress(
    JNIEnv* env, jobject obj, jobject byte_buffer, jlong nativeAudioRecord) {
  webrtc::AudioRecordJni* this_object =
      reinterpret_cast<webrtc::AudioRecordJni*> (nativeAudioRecord);
  this_object->OnCacheDirectBufferAddress(env, byte_buffer);
}

void AudioRecordJni::OnCacheDirectBufferAddress(
    JNIEnv* env, jobject byte_buffer) {
  ALOGD("OnCacheDirectBufferAddress");
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  RTC_DCHECK(!direct_buffer_address_);
  direct_buffer_address_ =
      env->GetDirectBufferAddress(byte_buffer);
  jlong capacity = env->GetDirectBufferCapacity(byte_buffer);
  ALOGD("direct buffer capacity: %lld", capacity);
  direct_buffer_capacity_in_bytes_ = static_cast<size_t>(capacity);
}

void JNICALL AudioRecordJni::DataIsRecorded(
  JNIEnv* env, jobject obj, jint length, jlong nativeAudioRecord) {
  webrtc::AudioRecordJni* this_object =
      reinterpret_cast<webrtc::AudioRecordJni*> (nativeAudioRecord);
  this_object->OnDataIsRecorded(length);
}

// This method is called on a high-priority thread from Java. The name of
// the thread is 'AudioRecordThread'.
void AudioRecordJni::OnDataIsRecorded(int length) {
  RTC_DCHECK(thread_checker_java_.CalledOnValidThread());
  if (!audio_device_buffer_) {
    ALOGE("AttachAudioBuffer has not been called!");
    return;
  }
  audio_device_buffer_->SetRecordedBuffer(direct_buffer_address_,
                                          frames_per_buffer_);
  // We provide one (combined) fixed delay estimate for the APM and use the
  // |playDelayMs| parameter only. Components like the AEC only sees the sum
  // of |playDelayMs| and |recDelayMs|, hence the distributions does not matter.
  audio_device_buffer_->SetVQEData(total_delay_in_milliseconds_,
                                   0,   // recDelayMs
                                   0);  // clockDrift
  if (audio_device_buffer_->DeliverRecordedData() == -1) {
    ALOGE("AudioDeviceBuffer::DeliverRecordedData failed!");
  }
}

}  // namespace webrtc
