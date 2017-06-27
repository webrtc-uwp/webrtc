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

#include <CoreFoundation/CoreFoundation.h>

#include "webrtc/base/gunit.h"

#include "scoped_cftyperef.h"

using rtc::RetainPolicy;
using rtc::ScopedCFTypeRef;
using rtc::AdoptCF;

TEST(ScopedCFTypeRefTest, DoesNotIncrementRetainCountByDefault) {
  int i = 1;
  CFNumberRef num = CFNumberCreate(nullptr, kCFNumberSInt64Type, &i);
  EXPECT_EQ(1, CFGetRetainCount(num));
  {
    auto scoped_num = ScopedCFTypeRef<CFNumberRef>(num);
    EXPECT_EQ(1, CFGetRetainCount(scoped_num.get()));
  }
}

TEST(ScopedCFTypeRefTest, IncrementsRetainCountWhenAdopting) {
  int i = 1;
  CFNumberRef num = CFNumberCreate(nullptr, kCFNumberSInt64Type, &i);
  EXPECT_EQ(1, CFGetRetainCount(num));
  {
    // Object is explicitly adopted.
    auto scoped_num = AdoptCF(num);
    EXPECT_EQ(2, CFGetRetainCount(scoped_num.get()));
  }
  EXPECT_EQ(1, CFGetRetainCount(num));
}

TEST(ScopedCFTypeRefTest, ResetWorksAsExpected) {
  int i = 1;
  CFNumberRef num1 = CFNumberCreate(nullptr, kCFNumberSInt64Type, &i);
  CFNumberRef num2 = CFNumberCreate(nullptr, kCFNumberSInt64Type, &i);
  EXPECT_EQ(1, CFGetRetainCount(num1));
  EXPECT_EQ(1, CFGetRetainCount(num2));
  {
    auto scoped_num = AdoptCF(num1);
    EXPECT_EQ(2, CFGetRetainCount(num1));
    scoped_num.reset(num2, RetainPolicy::RETAIN);
    EXPECT_EQ(1, CFGetRetainCount(num1));
    EXPECT_EQ(2, CFGetRetainCount(num2));
  }
  EXPECT_EQ(1, CFGetRetainCount(num1));
  EXPECT_EQ(1, CFGetRetainCount(num2));
}

TEST(ScopedCFTypeRefTest, AssignmentDoesNotIncreseRetainCount) {
  int i = 1;
  CFNumberRef num1 = CFNumberCreate(nullptr, kCFNumberSInt64Type, &i);
  CFNumberRef num2 = CFNumberCreate(nullptr, kCFNumberSInt64Type, &i);
  {
    ScopedCFTypeRef<CFNumberRef> scoped_num;
    // Assignment assumes retained object.
    scoped_num = num1;
    EXPECT_EQ(1, CFGetRetainCount(num1));
    // Unless specifically asked to adopt it.
    scoped_num = AdoptCF(num2);
    // num1 should be reclaimed now.
    EXPECT_EQ(2, CFGetRetainCount(num2));
  }
  EXPECT_EQ(1, CFGetRetainCount(num2));
}
