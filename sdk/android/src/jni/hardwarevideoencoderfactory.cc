/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/hardwarevideoencoderfactory.h"

#include "sdk/android/src/jni/jni_helpers.h"
#include "sdk/android/src/jni/videoencoderfactorywrapper.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {
namespace jni {

namespace {
const char kH264HighProfileFieldTrial[] = "WebRTC-H264HighProfile";
const char kIntelVP8FieldTrial[] = "WebRTC-WebRTC-IntelVP8";
}  // namespace

cricket::WebRtcVideoEncoderFactory* CreateHardwareVideoEncoderFactory(
    JNIEnv* jni,
    jobject shared_context) {
  jclass factory_class =
      jni->FindClass("org/webrtc/HardwareVideoEncoderFactory");
  jmethodID factory_constructor = jni->GetMethodID(
      factory_class, "<init>", "(Lorg/webrtc/EglBase$Context;ZZ)V");
  jboolean enable_intel_vp8_encoder =
      field_trial::IsEnabled(kIntelVP8FieldTrial);
  jboolean enable_h264_high_profile =
      field_trial::IsEnabled(kH264HighProfileFieldTrial);
  jobject factory_object =
      jni->NewObject(factory_class, factory_constructor, shared_context,
                     enable_intel_vp8_encoder, enable_h264_high_profile);
  return new VideoEncoderFactoryWrapper(jni, factory_object);
}

}  // namespace jni
}  // namespace webrtc
