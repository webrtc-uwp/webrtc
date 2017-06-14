/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/sdk/android/src/jni/videodecoderwrapper.h"

#include "webrtc/api/video/i420_buffer.h"
#include "webrtc/api/video/video_frame.h"
#include "webrtc/base/logging.h"
#include "webrtc/modules/video_coding/include/video_codec_interface.h"
#include "webrtc/sdk/android/src/jni/classreferenceholder.h"

using webrtc::VideoFrame;
using webrtc::I420Buffer;
using webrtc::VideoRotation;

namespace webrtc_jni {

VideoDecoderWrapper::VideoDecoderWrapper(JNIEnv* jni, jobject decoder)
    : android_video_buffer_factory_(jni),
      decoder_(jni, decoder),
      encoded_image_class_(jni, FindClass(jni, "org/webrtc/EncodedImage")),
      frame_type_class_(jni,
                        FindClass(jni, "org/webrtc/EncodedImage$FrameType")),
      settings_class_(jni, FindClass(jni, "org/webrtc/VideoDecoder$Settings")),
      video_frame_class_(jni, FindClass(jni, "org/webrtc/VideoFrame")),
      integer_class_(jni, jni->FindClass("java/lang/Integer")) {
  encoded_image_constructor_ =
      jni->GetMethodID(*encoded_image_class_, "<init>",
                       "(Ljava/nio/ByteBuffer;IIJLorg/webrtc/"
                       "EncodedImage$FrameType;IZLjava/lang/Integer;)V");
  settings_constructor_ = jni->GetMethodID(*settings_class_, "<init>", "(I)V");

  empty_frame_field_ = jni->GetStaticFieldID(
      *frame_type_class_, "EmptyFrame", "Lorg/webrtc/EncodedImage$FrameType;");
  video_frame_key_field_ =
      jni->GetStaticFieldID(*frame_type_class_, "VideoFrameKey",
                            "Lorg/webrtc/EncodedImage$FrameType;");
  video_frame_delta_field_ =
      jni->GetStaticFieldID(*frame_type_class_, "VideoFrameDelta",
                            "Lorg/webrtc/EncodedImage$FrameType;");

  video_frame_get_timestamp_ns_method_ =
      jni->GetMethodID(*video_frame_class_, "getTimestampNs", "()J");

  jclass decoder_class = jni->GetObjectClass(decoder);
  init_decode_method_ =
      jni->GetMethodID(decoder_class, "initDecode",
                       "(Lorg/webrtc/VideoDecoder$Settings;Lorg/webrtc/"
                       "VideoDecoder$Callback;)V");
  release_method_ = jni->GetMethodID(decoder_class, "release", "()V");
  decode_method_ = jni->GetMethodID(
      decoder_class, "decode",
      "(Lorg/webrtc/EncodedImage;Lorg/webrtc/VideoDecoder$DecodeInfo;)V");
  get_prefers_late_decoding_method_ =
      jni->GetMethodID(decoder_class, "getPrefersLateDecoding", "()Z");
  get_implementation_name_method_ = jni->GetMethodID(
      decoder_class, "getImplementationName", "()Ljava/lang/String;");

  integer_constructor_ = jni->GetMethodID(*integer_class_, "<init>", "(I)V");
  int_value_method_ = jni->GetMethodID(*integer_class_, "intValue", "()I");
}

int32_t VideoDecoderWrapper::InitDecode(
    const webrtc::VideoCodec* codec_settings,
    int32_t number_of_cores) {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);

  jobject settings =
      jni->NewObject(*settings_class_, settings_constructor_, number_of_cores);

  jclass callback_class =
      FindClass(jni, "org/webrtc/VideoDecoderWrapperCallback");
  jmethodID callback_constructor =
      jni->GetMethodID(callback_class, "<init>", "(J)V");
  jobject callback = jni->NewObject(callback_class, callback_constructor,
                                    reinterpret_cast<jlong>(this));

  jni->CallVoidMethod(*decoder_, init_decode_method_, settings, callback);
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t VideoDecoderWrapper::Decode(
    const webrtc::EncodedImage& input_image,
    bool missing_frames,
    const webrtc::RTPFragmentationHeader* fragmentation,
    const webrtc::CodecSpecificInfo* codec_specific_info,
    int64_t render_time_ms) {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);

  FrameExtraInfo frame_extra_info;
  frame_extra_info.capture_time_ms = input_image.capture_time_ms_;
  frame_extra_info.timestamp_rtp = input_image._timeStamp;
  frame_extra_infos_.push_back(frame_extra_info);

  jobject jinput_image =
      ConvertEncodedImageToJavaEncodedImage(jni, input_image);
  jni->CallVoidMethod(*decoder_, decode_method_, jinput_image, nullptr);
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t VideoDecoderWrapper::RegisterDecodeCompleteCallback(
    webrtc::DecodedImageCallback* callback) {
  callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t VideoDecoderWrapper::Release() {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);
  jni->CallVoidMethod(*decoder_, release_method_);
  frame_extra_infos_.clear();
  return WEBRTC_VIDEO_CODEC_OK;
}

bool VideoDecoderWrapper::PrefersLateDecoding() const {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);
  return jni->CallBooleanMethod(*decoder_, get_prefers_late_decoding_method_);
}

const char* VideoDecoderWrapper::ImplementationName() const {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);
  jstring jname = reinterpret_cast<jstring>(
      jni->CallObjectMethod(*decoder_, get_implementation_name_method_));
  return JavaToStdString(jni, jname).c_str();
}

void VideoDecoderWrapper::OnDecodedFrame(JNIEnv* jni,
                                         jobject jframe,
                                         jobject jdecode_time_ms,
                                         jobject jqp) {
  const jlong capture_time_ns =
      jni->CallLongMethod(jframe, video_frame_get_timestamp_ns_method_);
  const uint32_t capture_time_ms = capture_time_ns / 1000 / 1000;
  FrameExtraInfo frame_extra_info;
  do {
    if (frame_extra_infos_.empty()) {
      LOG(LS_WARNING) << "Java decoder produced an unexpected frame.";
      return;
    }

    frame_extra_info = frame_extra_infos_.front();
    frame_extra_infos_.pop_front();
  } while (frame_extra_info.capture_time_ms != capture_time_ms);

  webrtc::VideoFrame frame = android_video_buffer_factory_.CreateFrame(
      jni, jframe, frame_extra_info.timestamp_rtp);

  rtc::Optional<int32_t> decoding_time_ms;
  if (jdecode_time_ms != nullptr) {
    decoding_time_ms = rtc::Optional<int32_t>(
        jni->CallIntMethod(jdecode_time_ms, int_value_method_));
  }

  rtc::Optional<uint8_t> qp;
  if (jqp != nullptr) {
    qp = rtc::Optional<uint8_t>(jni->CallIntMethod(jqp, int_value_method_));
  }

  callback_->Decoded(frame, decoding_time_ms, rtc::Optional<uint8_t>());
}

jobject VideoDecoderWrapper::ConvertEncodedImageToJavaEncodedImage(
    JNIEnv* jni,
    const webrtc::EncodedImage& image) {
  jobject buffer = jni->NewDirectByteBuffer(image._buffer, image._length);
  jfieldID frame_type_field;
  switch (image._frameType) {
    case webrtc::kEmptyFrame:
      frame_type_field = empty_frame_field_;
      break;
    case webrtc::kVideoFrameKey:
      frame_type_field = video_frame_key_field_;
      break;
    case webrtc::kVideoFrameDelta:
      frame_type_field = video_frame_delta_field_;
      break;
    default:
      RTC_NOTREACHED();
  }
  jobject frame_type =
      jni->GetStaticObjectField(*frame_type_class_, frame_type_field);
  jobject qp = nullptr;
  if (image.qp_ != -1) {
    qp = jni->NewObject(*integer_class_, integer_constructor_, image.qp_);
  }
  return jni->NewObject(*encoded_image_class_, encoded_image_constructor_,
                        buffer, (jint)image._encodedWidth,
                        (jint)image._encodedHeight,
                        (jlong)image.capture_time_ms_, frame_type,
                        (jint)image.rotation_, image._completeFrame, qp);
}

JOW(void, VideoDecoderWrapperCallback_nativeOnDecodedFrame)
(JNIEnv* jni,
 jclass,
 jlong jnative_decoder,
 jobject jframe,
 jobject jdecode_time_ms,
 jobject jqp) {
  VideoDecoderWrapper* native_decoder =
      reinterpret_cast<VideoDecoderWrapper*>(jnative_decoder);
  native_decoder->OnDecodedFrame(jni, jframe, jdecode_time_ms, jqp);
}

}  // namespace webrtc_jni
