// Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
//

#include "webrtc/system_wrappers/include/metrics.h"

// Default implementation of histogram methods for WebRTC clients that do not
// want to provide their own implementation.

namespace webrtc {
namespace metrics {

Histogram* HistogramFactoryGetCounts(const std::string& name, int min, int max,
    int bucket_count) { return NULL; }

Histogram* HistogramFactoryGetEnumeration(const std::string& name,
    int boundary) { return NULL; }

void HistogramAdd(
    Histogram* histogram_pointer, const std::string& name, int sample) {}

}  // namespace metrics

// On WinRT we wrap all unit tests in a single executable.
// The fact that metrics_defaults.cc doesn't define all functions
// found in histogram.cc results in linking errors.
#ifdef WINRT
//namespace test {
//  int LastHistogramSample(const std::string& name) { return 0; }
//
//  int NumHistogramSamples(const std::string& name) { return 0; }
//
//  void ClearHistograms() {}
//}  // namespace test
#endif
}  // namespace webrtc

