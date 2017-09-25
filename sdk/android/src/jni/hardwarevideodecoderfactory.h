/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_ANDROID_SRC_JNI_HARDWAREVIDEODECODERFACTORY_H_
#define SDK_ANDROID_SRC_JNI_HARDWAREVIDEODECODERFACTORY_H_

#include <jni.h>

#include "media/engine/webrtcvideodecoderfactory.h"

namespace webrtc {
namespace jni {

// Helper method for creating HardwareVideoDecoderFactory from C++.
// Currently used for testing but could also be used by native C++ clients.
cricket::WebRtcVideoDecoderFactory* CreateHardwareVideoDecoderFactory(
    JNIEnv* jni,
    jobject shared_context);

}  // namespace jni
}  // namespace webrtc

#endif  // SDK_ANDROID_SRC_JNI_HARDWAREVIDEODECODERFACTORY_H_
