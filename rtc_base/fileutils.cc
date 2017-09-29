/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/fileutils.h"

#include "rtc_base/arraysize.h"
#include "rtc_base/checks.h"
#include "rtc_base/pathutils.h"
#include "rtc_base/stringutils.h"

#if defined(WEBRTC_WIN)
#include "rtc_base/win32filesystem.h"
#else
#include "rtc_base/unixfilesystem.h"
#endif

#if !defined(WEBRTC_WIN)
#define MAX_PATH 260
#endif

namespace rtc {

FilesystemInterface* Filesystem::default_filesystem_ = nullptr;

FilesystemInterface *Filesystem::EnsureDefaultFilesystem() {
  if (!default_filesystem_) {
#if defined(WEBRTC_WIN)
    default_filesystem_ = new Win32Filesystem();
#else
    default_filesystem_ = new UnixFilesystem();
#endif
  }
  return default_filesystem_;
}

}  // namespace rtc
