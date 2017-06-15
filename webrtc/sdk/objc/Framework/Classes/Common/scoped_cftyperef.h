/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#ifndef WEBRTC_SDK_OBJC_FRAMEWORK_CLASSES_COMMON_SCOPED_CFTYPEREF_H_
#define WEBRTC_SDK_OBJC_FRAMEWORK_CLASSES_COMMON_SCOPED_CFTYPEREF_H_

#include <CoreFoundation/CoreFoundation.h>

namespace rtc {

template <typename T>
class ScopedCFTypeRef {
 public:
  ScopedCFTypeRef() : ptr_(nullptr) {}
  ScopedCFTypeRef(T ptr) : ptr_(ptr) {}
  ~ScopedCFTypeRef() {
    if (ptr_) {
      CFRelease(ptr_);
      ptr_ = nullptr;
    }
  }
  T get() const { return ptr_; }
  T operator->() const { return ptr_; }
  explicit operator bool() const { return ptr_; }

  bool operator!() const { return !ptr_; }
  ScopedCFTypeRef<T>& operator=(const T& rhs) {
    ptr_ = rhs;
    return *this;
  }
  ScopedCFTypeRef<T>& operator=(const ScopedCFTypeRef<T>& rhs) {
    ptr_ = rhs.ptr_;
    return *this;
  }
  ScopedCFTypeRef<T>& operator=(const T&& rhs) {
    ptr_ = std::move(rhs);
    return *this;
  }

 private:
  T ptr_;
};
}

#endif  // WEBRTC_SDK_OBJC_FRAMEWORK_CLASSES_COMMON_SCOPED_CFTYPEREF_H_
