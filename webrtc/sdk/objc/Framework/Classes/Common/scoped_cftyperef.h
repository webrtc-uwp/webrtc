/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
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

enum class scoped_policy { ADOPT, ASSUME };

template <typename T>
class ScopedCFTypeRef {
 public:
  ScopedCFTypeRef() : ptr_(nullptr) {}
  explicit ScopedCFTypeRef(T ptr) : ptr_(ptr) {}
  ScopedCFTypeRef<T>(T ptr, scoped_policy policy) : ScopedCFTypeRef(ptr) {
    if (ptr_ && policy == scoped_policy::ADOPT)
      CFRetain(ptr_);
  }
  ~ScopedCFTypeRef() {
    if (ptr_) {
      CFRelease(ptr_);
    }
  }
  T get() const { return ptr_; }
  T operator->() const { return ptr_; }
  explicit operator bool() const { return ptr_; }

  bool operator!() const { return !ptr_; }

  ScopedCFTypeRef<T>& operator=(const T& rhs) {
    if (ptr_)
      CFRelease(ptr_);
    ptr_ = rhs;
    return *this;
  }
  ScopedCFTypeRef<T>& operator=(const ScopedCFTypeRef<T>& rhs) {
    reset(rhs.get(), scoped_policy::ADOPT);
    return *this;
  }
  ScopedCFTypeRef<T>& operator=(const T&& rhs) {
    ptr_ = std::move(rhs);
    return *this;
  }
  // This is intended to take ownership of objects that are
  // created by pass-by-pointer initializers.
  T* InitializeInto() {
    RTC_DCHECK(!ptr_);
    return &ptr_;
  }

  void reset(T ptr, scoped_policy policy = scoped_policy::ASSUME) {
    if (ptr && policy == scoped_policy::ADOPT)
      CFRetain(ptr);
    if (ptr_)
      CFRelease(ptr_);
    ptr_ = ptr;
  }

 private:
  __unsafe_unretained T ptr_;
};

template <typename T>
static ScopedCFTypeRef<T> AdoptCF(T cftype) {
  return ScopedCFTypeRef<T>(cftype, scoped_policy::ADOPT);
}

template <typename T>
static ScopedCFTypeRef<T> ScopedCF(T cftype) {
  return ScopedCFTypeRef<T>(cftype);
}

}  // namespace rtc

#endif  // WEBRTC_SDK_OBJC_FRAMEWORK_CLASSES_COMMON_SCOPED_CFTYPEREF_H_
