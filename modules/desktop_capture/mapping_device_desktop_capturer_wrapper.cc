/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/mapping_device_desktop_capturer_wrapper.h"

#include <algorithm>
#include <set>
#include <string>
#include <utility>

#include "rtc_base/checks.h"

namespace webrtc {

MappingDeviceDesktopCapturerWrapper::MappingDeviceDesktopCapturerWrapper(
    std::unique_ptr<DesktopCapturer> base_capturer)
    : DesktopCapturerWrapper(std::move(base_capturer)) {}

MappingDeviceDesktopCapturerWrapper::
~MappingDeviceDesktopCapturerWrapper() = default;

bool MappingDeviceDesktopCapturerWrapper::GetSourceList(SourceList* sources) {
  std::vector<DetailSource> merged;
  if (!GetMergedSources(&merged)) {
    return false;
  }
  for (const DetailSource& source : merged) {
    sources->push_back({ source.id, source.title });
  }
  return true;
}

bool MappingDeviceDesktopCapturerWrapper::SelectSource(SourceId id) {
  std::vector<DetailSource> merged;
  if (!GetMergedSources(&merged)) {
    return false;
  }

  const auto it = std::find_if(merged.begin(),
                               merged.end(),
                               [id](const DetailSource& source) -> bool {
                                 return id == source.id;
                               });
  if (it == merged.end()) {
    return false;
  }

  std::vector<DetailSource> internal_sources;
  if (!GetInternalSources(&internal_sources)) {
    return false;
  }

  const std::string& name = it->name;
  const auto internal_source = std::find_if(
      internal_sources.begin(),
      internal_sources.end(),
      [&name](const DetailSource& source) -> bool {
        return name == source.name;
      });
  if (internal_source == internal_sources.end()) {
    return false;
  }

  return base_capturer_->SelectSource(internal_source->id);
}

bool MappingDeviceDesktopCapturerWrapper::GetMergedSources(
    std::vector<DetailSource>* output) {
  std::vector<DetailSource> internal_sources;
  if (!GetInternalSources(&internal_sources)) {
    return false;
  }

  std::vector<DetailSource> external_sources;
  if (!GetExternalSources(&external_sources)) {
    return false;
  }

  SourceList list;
  if (!base_capturer_->GetSourceList(&list)) {
    return false;
  }

  SourceId max_source_id = 0;
  for (const DetailSource& source : external_sources) {
    max_source_id = std::max(source.id, max_source_id);
  }

  std::set<std::string> names;
  for (const DetailSource& source : internal_sources) {
    const std::string& name = source.name;
    if (name.empty()) {
      continue;
    }

    if (!names.insert(name).second) {
      continue;
    }

    std::string title;
    {
      const SourceId& id = source.id;
      const auto it = std::find_if(list.begin(),
                                   list.end(),
                                   [&id](const Source& source) -> bool {
                                     return id == source.id;
                                   });
      if (it != list.end()) {
        title = it->title;
      }
    }

    SourceId id;
    {
      const auto it = std::find_if(external_sources.begin(),
                                   external_sources.end(),
                                   [&name](const DetailSource& source) -> bool {
                                     return name == source.name;
                                   });
      if (it == external_sources.end()) {
        max_source_id++;
        id = max_source_id;
      } else {
        id = it->id;
      }
    }

    output->push_back({ id, name, title });
  }

  return true;
}

}  // namespace webrtc
