/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "RTCUIApplicationStatusObserver.h"

#if defined(WEBRTC_IOS)

#import <UIKit/UIKit.h>

@implementation RTCUIApplicationStatusObserver {
  id<NSObject> _activeObserver;
  id<NSObject> _backgroundObserver;
}

@synthesize state;

- (id)init {
  if (self = [super init]) {
    NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
    __weak RTCUIApplicationStatusObserver *weakSelf = self;
    _activeObserver = [center addObserverForName:UIApplicationDidBecomeActiveNotification
                                          object:nil
                                           queue:[NSOperationQueue mainQueue]
                                      usingBlock:^(NSNotification *note) {
                                        weakSelf.state =
                                            [UIApplication sharedApplication].applicationState;
                                      }];

    _backgroundObserver = [center addObserverForName:UIApplicationDidEnterBackgroundNotification
                                              object:nil
                                               queue:[NSOperationQueue mainQueue]
                                          usingBlock:^(NSNotification *note) {
                                            weakSelf.state =
                                                [UIApplication sharedApplication].applicationState;
                                          }];

    self.state = UIApplicationStateActive;
    dispatch_async(dispatch_get_main_queue(), ^{
      self.state = [UIApplication sharedApplication].applicationState;
    });
  }

  return self;
}

- (void)dealloc {
  NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
  [center removeObserver:_activeObserver];
  [center removeObserver:_backgroundObserver];
}

- (BOOL)isApplicationActive {
  return self.state == UIApplicationStateActive;
}

@end

#endif  // WEBRTC_IOS
