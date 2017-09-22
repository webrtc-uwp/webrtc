/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_BASE_REFCOUNTER_H_
#define RTC_BASE_REFCOUNTER_H_

#include "rtc_base/atomicops.h"

namespace webrtc {
namespace webrtc_impl {

class RefCounter {
 public:
  // TODO(nisse): Change return value to void
  int AddRef() const { return rtc::AtomicOps::Increment(&ref_count_); }

  // Returns zero if this was the last reference, and the resource
  // associated with the reference counter can be deleted.
  // TODO(nisse): Change return value to bool.
  int Release() const { return rtc::AtomicOps::Decrement(&ref_count_); }

  // Return whether the reference count is one. If the reference count is used
  // in the conventional way, a reference count of 1 implies that the current
  // thread owns the reference and no other thread shares it. This call
  // performs the test for a reference count of one, and performs the memory
  // barrier needed for the owning thread to act on the object, knowing that it
  // has exclusive access to the object.

  bool HasOneRef() const {
    return rtc::AtomicOps::AcquireLoad(&ref_count_) == 1;
  }

 private:
  mutable volatile int ref_count_ = 0;
};

}  // namespace webrtc_impl
}  // namespace webrtc

#endif  // RTC_BASE_REFCOUNTER_H_
