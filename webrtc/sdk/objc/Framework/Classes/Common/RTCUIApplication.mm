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

class RTCUIApplicationStatusObserver::Impl {
 public:
  id<NSObject> _activeObserver;
  id<NSObject> _backgroundObserver;
  UIApplicationState _state;
};

RTCUIApplicationStatusObserver::RTCUIApplicationStatusObserver() {
  impl.reset(new Impl());
  NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
  impl->_activeObserver =
      [center addObserverForName:UIApplicationDidBecomeActiveNotification
                          object:nil
                           queue:[NSOperationQueue mainQueue]
                      usingBlock:^(NSNotification *note) {
                        impl->_state = [UIApplication sharedApplication].applicationState;
                      }];

  impl->_backgroundObserver =
      [center addObserverForName:UIApplicationDidEnterBackgroundNotification
                          object:nil
                           queue:[NSOperationQueue mainQueue]
                      usingBlock:^(NSNotification *note) {
                        impl->_state = [UIApplication sharedApplication].applicationState;
                      }];

  impl->_state = UIApplicationStateActive;
  dispatch_async(dispatch_get_main_queue(), ^{
    impl->_state = [UIApplication sharedApplication].applicationState;
  });
}

RTCUIApplicationStatusObserver::~RTCUIApplicationStatusObserver() {
  NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
  [center removeObserver:impl->_activeObserver];
  [center removeObserver:impl->_backgroundObserver];
}

bool RTCUIApplicationStatusObserver::IsApplicationActive() {
  return impl->_state == UIApplicationStateActive;
}

#endif // WEBRTC_IOS
