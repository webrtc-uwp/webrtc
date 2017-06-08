/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "RTCUIApplication.h"

#if defined(WEBRTC_IOS)

#import <UIKit/UIKit.h>

RTCUIApplicationStatusObserver::RTCUIApplicationStatusObserver() {
  NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
  _activeObserver = (__bridge void *)[center
      addObserverForName:UIApplicationDidBecomeActiveNotification
                  object:nil
                   queue:[NSOperationQueue mainQueue]
              usingBlock:^(NSNotification *note) {
                _state = [UIApplication sharedApplication].applicationState;
              }];

  _backgroundObserver = (__bridge void *)[center
      addObserverForName:UIApplicationDidEnterBackgroundNotification
                  object:nil
                   queue:[NSOperationQueue mainQueue]
              usingBlock:^(NSNotification *note) {
                _state = [UIApplication sharedApplication].applicationState;
              }];

  dispatch_async(dispatch_get_main_queue(), ^{
    _state = [UIApplication sharedApplication].applicationState;
  });
}

RTCUIApplicationStatusObserver::~RTCUIApplicationStatusObserver() {
  NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
  [center removeObserver:(__bridge id)_activeObserver];
  [center removeObserver:(__bridge id)_backgroundObserver];
}

bool RTCUIApplicationStatusObserver::IsApplicationActive() {
  return _state == UIApplicationStateActive;
}

#endif // WEBRTC_IOS
