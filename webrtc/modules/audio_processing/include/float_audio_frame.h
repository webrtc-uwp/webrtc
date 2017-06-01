/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_INCLUDE_FLOAT_AUDIO_FRAME_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_INCLUDE_FLOAT_AUDIO_FRAME_H_

#include "webrtc/base/array_view.h"

// Class to pass audio data in float** format. This is to avoid
// dependence on AudioBuffer, and avoid problems associated with
// rtc::ArrayView<rtc::ArrayView>.
template <typename T>
class FloatAudioFrame {
 public:
  // |num_channels| and |channel_size| describe the float**
  // |audio_samples|. |audio_samples| is assumed to point to a
  // two-dimensional |num_channels * channel_size| array of floats.
  FloatAudioFrame(T* const* audio_samples,
                  size_t num_channels,
                  size_t channel_size)
      : audio_samples_(audio_samples),
        num_channels_(num_channels),
        channel_size_(channel_size) {}

  FloatAudioFrame() = delete;

  size_t num_channels() const { return num_channels_; }

  rtc::ArrayView<float> channel(size_t idx) {
    RTC_DCHECK_LE(0, idx);
    RTC_DCHECK_LE(idx, num_channels_);
    return rtc::ArrayView<T>(audio_samples_[idx], channel_size_);
  }

  rtc::ArrayView<const T> channel(size_t idx) const {
    RTC_DCHECK_LE(0, idx);
    RTC_DCHECK_LE(idx, num_channels_);
    return rtc::ArrayView<const T>(audio_samples_[idx], channel_size_);
  }

 private:
  T* const* audio_samples_;
  size_t num_channels_;
  size_t channel_size_;
};

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_INCLUDE_FLOAT_AUDIO_FRAME_H_
