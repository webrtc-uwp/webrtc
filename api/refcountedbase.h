/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef API_REFCOUNTEDBASE_H_
#define API_REFCOUNTEDBASE_H_

#include "rtc_base/refcounter.h"

namespace rtc {

class RefCountedBase {
 public:
  RefCountedBase() : ref_count_(0) {}
  void AddRef() const { ref_count_.IncRef(); }
  void Release() const {
    if (ref_count_.DecRef()) {
      delete this;
    }
  }

 protected:
  virtual ~RefCountedBase() = default;

 private:
  mutable webrtc::webrtc_impl::RefCounter ref_count_;
};

}  // namespace rtc

#endif  // API_REFCOUNTEDBASE_H_
