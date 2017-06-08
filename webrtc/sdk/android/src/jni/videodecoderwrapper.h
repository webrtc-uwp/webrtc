/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SDK_ANDROID_SRC_JNI_VIDEODECODERWRAPPER_H_
#define WEBRTC_SDK_ANDROID_SRC_JNI_VIDEODECODERWRAPPER_H_

#include <jni.h>

#include "webrtc/api/video_codecs/video_decoder.h"
#include "webrtc/sdk/android/src/jni/jni_helpers.h"

namespace webrtc_jni {

// Wraps a Java decoder and forwards all calls to it. Receives frames from the
// Java decoder and forwards them back.
class VideoDecoderWrapper : public webrtc::VideoDecoder {
 public:
  VideoDecoderWrapper(JNIEnv* jni, jobject decoder);

  int32_t InitDecode(const webrtc::VideoCodec* codec_settings,
                     int32_t number_of_cores) override;

  int32_t Decode(const webrtc::EncodedImage& input_image,
                 bool missing_frames,
                 const webrtc::RTPFragmentationHeader* fragmentation,
                 const webrtc::CodecSpecificInfo* codec_specific_info = NULL,
                 int64_t render_time_ms = -1) override;

  int32_t RegisterDecodeCompleteCallback(
      webrtc::DecodedImageCallback* callback) override;

  int32_t Release() override;

  // Returns true if the decoder prefer to decode frames late.
  // That is, it can not decode infinite number of frames before the decoded
  // frame is consumed.
  bool PrefersLateDecoding() const override;

  const char* ImplementationName() const override;

  void OnDecodedFrame(JNIEnv* jni,
                      jobject jframe,
                      jobject jdecode_time_ms,
                      jobject jqp);

 private:
  const ScopedGlobalRef<jobject> decoder_;
  const ScopedGlobalRef<jclass> encoded_image_class_;
  const ScopedGlobalRef<jclass> settings_class_;
  const ScopedGlobalRef<jclass> video_frame_class_;

  jmethodID encoded_image_constructor_;
  jmethodID settings_constructor_;

  jmethodID video_frame_get_timestamp_rtp_method_;

  jmethodID init_decode_method_;
  jmethodID release_method_;
  jmethodID decode_method_;
  jmethodID get_prefers_late_decoding_method_;
  jmethodID get_implementation_name_method_;

  webrtc::DecodedImageCallback* callback_;

  jobject ConvertEncodedImageToJavaEncodedImage(
      JNIEnv* jni,
      const webrtc::EncodedImage& image);
};

}  // namespace webrtc_jni

#endif  // WEBRTC_SDK_ANDROID_SRC_JNI_VIDEODECODERWRAPPER_H_
