/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_FILEUTILS_H_
#define RTC_BASE_FILEUTILS_H_

#include <string>

#if !defined(WEBRTC_WIN)
#include <dirent.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include "rtc_base/checks.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/platform_file.h"

namespace rtc {

class Pathname;

class FilesystemInterface {
 public:
  virtual ~FilesystemInterface() {}

  // This will attempt to delete the path located at filename.
  // It DCHECKs and returns false if the path points to a folder or a
  // non-existent file.
  virtual bool DeleteFile(const Pathname &filename) = 0;

  // This moves a file from old_path to new_path, where "old_path" is a
  // plain file. This DCHECKs and returns false if old_path points to a
  // directory, and returns true if the function succeeds.
  virtual bool MoveFile(const Pathname &old_path, const Pathname &new_path) = 0;

  // Returns true if pathname refers to a directory
  virtual bool IsFolder(const Pathname& pathname) = 0;

  // Returns true if pathname refers to a file
  virtual bool IsFile(const Pathname& pathname) = 0;

  virtual std::string TempFilename(const Pathname &dir,
                                   const std::string &prefix) = 0;

  // Determines the size of the file indicated by path.
  virtual bool GetFileSize(const Pathname& path, size_t* size) = 0;
};

class Filesystem {
 public:
  static FilesystemInterface *default_filesystem() {
    RTC_DCHECK(default_filesystem_);
    return default_filesystem_;
  }

  static void set_default_filesystem(FilesystemInterface *filesystem) {
    default_filesystem_ = filesystem;
  }

  static FilesystemInterface *swap_default_filesystem(
      FilesystemInterface *filesystem) {
    FilesystemInterface *cur = default_filesystem_;
    default_filesystem_ = filesystem;
    return cur;
  }

  static bool DeleteFile(const Pathname &filename) {
    return EnsureDefaultFilesystem()->DeleteFile(filename);
  }

  static bool MoveFile(const Pathname &old_path, const Pathname &new_path) {
    return EnsureDefaultFilesystem()->MoveFile(old_path, new_path);
  }

  static bool IsFolder(const Pathname& pathname) {
    return EnsureDefaultFilesystem()->IsFolder(pathname);
  }

  static bool IsFile(const Pathname &pathname) {
    return EnsureDefaultFilesystem()->IsFile(pathname);
  }

  static std::string TempFilename(const Pathname &dir,
                                  const std::string &prefix) {
    return EnsureDefaultFilesystem()->TempFilename(dir, prefix);
  }

  static bool GetFileSize(const Pathname& path, size_t* size) {
    return EnsureDefaultFilesystem()->GetFileSize(path, size);
  }

 private:
  static FilesystemInterface* default_filesystem_;

  static FilesystemInterface *EnsureDefaultFilesystem();
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(Filesystem);
};

}  // namespace rtc

#endif  // RTC_BASE_FILEUTILS_H_
