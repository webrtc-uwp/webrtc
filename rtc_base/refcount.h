/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_BASE_REFCOUNT_H_
#define RTC_BASE_REFCOUNT_H_

#include "rtc_base/refcountedobject.h"

namespace rtc {

// Interface for reference counted objects.
//
// AddRef() creates a new reference to the object. The caller must
// already have a reference, or have borrowed one. (A newly created
// object is a special case: there, the code that creates the object
// should immediately call AddRef(), bringing the reference count from
// 0 to 1, typically by constructing an rtc::scoped_refptr).
//
// Release() releases a reference to the object; the caller now has
// one less reference than before the call. Returns true if the number
// of references dropped to zero because of this (in which case the
// object destroys itself).
//
// The caller of Release() must treat it in the same way as a delete
// operation. Regardless of the return value from Release(), the
// caller mustn't access the object. The object might still be alive,
// due to references held by other users of the object, but the object
// can go away at any time, e.g., as the result of another thread
// calling Release().
//
// Calling AddRef() and Release() explicitly is discouraged. It's
// recommended to use rtc::scoped_refptr to manage all pointers to
// reference counted objects.
class RefCountInterface {
 public:
  virtual void AddRef() const = 0;
  virtual bool Release() const = 0;

 protected:
  virtual ~RefCountInterface() {}
};

}  // namespace rtc

#endif  // RTC_BASE_REFCOUNT_H_
