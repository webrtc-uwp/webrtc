/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/hardwarevideodecoderfactory.h"

#include "sdk/android/src/jni/jni_helpers.h"
#include "sdk/android/src/jni/videodecoderfactorywrapper.h"

namespace webrtc {
namespace jni {

cricket::WebRtcVideoDecoderFactory* CreateHardwareVideoDecoderFactory(
    JNIEnv* jni,
    jobject shared_context) {
  jclass factory_class =
      jni->FindClass("org/webrtc/HardwareVideoDecoderFactory");
  jmethodID factory_constructor = jni->GetMethodID(
      factory_class, "<init>", "(Lorg/webrtc/EglBase$Context;)V");
  jobject factory_object =
      jni->NewObject(factory_class, factory_constructor, shared_context);
  return new VideoDecoderFactoryWrapper(jni, factory_object);
}

}  // namespace jni
}  // namespace webrtc
