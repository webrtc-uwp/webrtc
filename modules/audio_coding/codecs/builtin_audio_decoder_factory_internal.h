/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_CODECS_BUILTIN_AUDIO_DECODER_FACTORY_INTERNAL_H_
#define MODULES_AUDIO_CODING_CODECS_BUILTIN_AUDIO_DECODER_FACTORY_INTERNAL_H_

#include "api/audio_codecs/audio_decoder_factory.h"
#include "rtc_base/scoped_ref_ptr.h"

namespace webrtc {

rtc::scoped_refptr<AudioDecoderFactory>
CreateBuiltinAudioDecoderFactoryInternal();

}  // namespace webrtc

#endif  // MODULES_AUDIO_CODING_CODECS_BUILTIN_AUDIO_DECODER_FACTORY_INTERNAL_H_
