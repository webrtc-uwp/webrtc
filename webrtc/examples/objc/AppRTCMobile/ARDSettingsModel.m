/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "ARDSettingsModel+Private.h"
#import "ARDSettingsStore.h"
#import "WebRTC/RTCMediaConstraints.h"

NS_ASSUME_NONNULL_BEGIN
static NSArray<NSString *> *videoResolutionsStaticValues() {
  return @[ @"640x480", @"960x540", @"1280x720" ];
}

static NSArray<NSString *> *videoCodecsStaticValues() {
  return @[ @"H264", @"VP8", @"VP9" ];
}

@interface ARDSettingsModel () {
  ARDSettingsStore *_settingsStore;
}
@end

@implementation ARDSettingsModel

+ (void)initialize {
  [ARDSettingsStore setDefaultsForVideoResolution:[self defaultVideoResolutionSetting]
                                       videoCodec:[self defaultVideoCodecSetting]
                                          bitrate:nil
                                        audioOnly:NO
                                    createAecDump:NO
                               useLevelController:NO
                             useManualAudioConfig:YES];
}

- (NSArray<NSString *> *)availableVideoResolutions {
  return videoResolutionsStaticValues();
}

- (NSString *)currentVideoResolutionSettingFromStore {
  return [[self settingsStore] videoResolution];
}

- (BOOL)storeVideoResolutionSetting:(NSString *)resolution {
  if (![[self availableVideoResolutions] containsObject:resolution]) {
    return NO;
  }
  [[self settingsStore] setVideoResolution:resolution];
  return YES;
}

- (NSArray<NSString *> *)availableVideoCodecs {
  return videoCodecsStaticValues();
}

- (NSString *)currentVideoCodecSettingFromStore {
  return [[self settingsStore] videoCodec];
}

- (BOOL)storeVideoCodecSetting:(NSString *)videoCodec {
  if (![[self availableVideoCodecs] containsObject:videoCodec]) {
    return NO;
  }
  [[self settingsStore] setVideoCodec:videoCodec];
  return YES;
}

- (nullable NSNumber *)currentMaxBitrateSettingFromStore {
  return [[self settingsStore] maxBitrate];
}

- (void)storeMaxBitrateSetting:(nullable NSNumber *)bitrate {
  [[self settingsStore] setMaxBitrate:bitrate];
}

- (BOOL)currentAudioOnlySettingFromStore {
  return [[self settingsStore] audioOnly];
}

- (void)storeAudioOnlySetting:(BOOL)audioOnly {
  [[self settingsStore] setAudioOnly:audioOnly];
}

- (BOOL)currentCreateAecDumpSettingFromStore {
  return [[self settingsStore] createAecDump];
}

- (void)storeCreateAecDumpSetting:(BOOL)createAecDump {
  [[self settingsStore] setCreateAecDump:createAecDump];
}

- (BOOL)currentUseLevelControllerSettingFromStore {
  return [[self settingsStore] useLevelController];
}

- (void)storeUseLevelControllerSetting:(BOOL)useLevelController {
  [[self settingsStore] setUseLevelController:useLevelController];
}

- (BOOL)currentUseManualAudioConfigSettingFromStore {
  return [[self settingsStore] useManualAudioConfig];
}

- (void)storeUseManualAudioConfigSetting:(BOOL)useManualAudioConfig {
  [[self settingsStore] setUseManualAudioConfig:useManualAudioConfig];
}

#pragma mark - Testable

- (ARDSettingsStore *)settingsStore {
  if (!_settingsStore) {
    _settingsStore = [[ARDSettingsStore alloc] init];
  }
  return _settingsStore;
}

- (int)currentVideoResolutionWidthFromStore {
  NSString *resolution = [self currentVideoResolutionSettingFromStore];

  return [self videoResolutionComponentAtIndex:0 inString:resolution];
}

- (int)currentVideoResolutionHeightFromStore {
  NSString *resolution = [self currentVideoResolutionSettingFromStore];
  return [self videoResolutionComponentAtIndex:1 inString:resolution];
}

#pragma mark -

+ (NSString *)defaultVideoResolutionSetting {
  return videoResolutionsStaticValues()[0];
}

+ (NSString *)defaultVideoCodecSetting {
  return videoCodecsStaticValues()[0];
}

- (int)videoResolutionComponentAtIndex:(int)index inString:(NSString *)resolution {
  if (index != 0 && index != 1) {
    return 0;
  }
  NSArray<NSString *> *components = [resolution componentsSeparatedByString:@"x"];
  if (components.count != 2) {
    return 0;
  }
  return components[index].intValue;
}

@end
NS_ASSUME_NONNULL_END
