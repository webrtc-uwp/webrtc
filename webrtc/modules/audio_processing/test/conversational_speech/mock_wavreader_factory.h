/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_TEST_CONVERSATIONAL_SPEECH_MOCK_WAVREADER_FACTORY_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_TEST_CONVERSATIONAL_SPEECH_MOCK_WAVREADER_FACTORY_H_

#include <memory>
#include <string>

#include "webrtc/modules/audio_processing/test/conversational_speech/wavreader_abstract_factory.h"
#include "webrtc/modules/audio_processing/test/conversational_speech/wavreader_interface.h"
#include "webrtc/test/gmock.h"

namespace webrtc {
namespace test {
namespace conversational_speech {

class MockWavReaderFactory : public WavReaderAbstractFactory {
 public:
  MockWavReaderFactory();
  // TODO(alessiob): add ctor that gets map string->(sr, #samples, #channels).
  ~MockWavReaderFactory();

  // TODO(alessiob): use ON_CALL to return MockWavReader with desired params.
  MOCK_CONST_METHOD1(Create, std::unique_ptr<WavReaderInterface>(
      const std::string&));

  // TODO(alessiob): add const ref to map (see ctor to add).
};

}  // namespace conversational_speech
}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_TEST_CONVERSATIONAL_SPEECH_MOCK_WAVREADER_FACTORY_H_
