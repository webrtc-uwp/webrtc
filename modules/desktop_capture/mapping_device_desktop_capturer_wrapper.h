/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_MAPPING_DEVICE_DESKTOP_CAPTURER_WRAPPER_H_
#define MODULES_DESKTOP_CAPTURE_MAPPING_DEVICE_DESKTOP_CAPTURER_WRAPPER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "modules/desktop_capture/desktop_capturer_wrapper.h"

namespace webrtc {

// Maps between string ("internal") and numeric ("external") source identifiers.
// This allows capturers operating on the same physical display hardware to be
// expose the same external source identifiers even if the underlying capture
// APIs assign them different SourceId. For example,
// FallbackDesktopCapturerWrapper requires the consistency between the SourceIds
// returned by two DesktopCapturer implementations. This may not be achievable
// for some implementations. So instead of the SourceId in integer format, this
// class uses the source name in string format to do the mapping.
class MappingDeviceDesktopCapturerWrapper : public DesktopCapturerWrapper {
 public:
  explicit MappingDeviceDesktopCapturerWrapper(
      std::unique_ptr<DesktopCapturer> base_capturer);
  ~MappingDeviceDesktopCapturerWrapper() override;

  // DesktopCapturerWrapper implementations.
  bool GetSourceList(SourceList* sources) final;
  bool SelectSource(SourceId id) final;

 protected:
  // A webrtc::Source structure with "name" field. Use |id| and |title| fields
  // to create a webrtc::Source.
  struct DetailSource {
    SourceId id;
    // A unique string to match between internal and external sources.
    std::string name;
    std::string title;
  };

  // Gets the names and SourceIds of the "external" sources. The SourceIds
  // returned by this function are expected to be outputted via GetSourceList()
  // function. DetailSource::title is ignored.
  virtual bool GetExternalSources(std::vector<DetailSource>* sources) = 0;

  // Gets the names and SourceIds of the "internal" sources. The SourceIds
  // returned by this function are expected to be sent to |base_capturer_|.
  // DetailSource::title is ignored.
  virtual bool GetInternalSources(std::vector<DetailSource>* sources) = 0;

 private:
  // Merges GetExternalSources() and GetInternalSrouces() and writes the result
  // into |output|. It does,
  // 1. Ignore any sources without a valid name from both inputs.
  // 2. Remove all the "external" only sources, i.e. the sources can be found
  // only in GetExternalSources() but not GetInternalSources().
  // 3. Add all the "internal" only sources with unconflicted source id.
  // This function returns false only when GetExternalSources() or
  // GetInternalSources() failed.
  bool GetMergedSources(std::vector<DetailSource>* output);
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_MAPPING_DEVICE_DESKTOP_CAPTURER_WRAPPER_H_
