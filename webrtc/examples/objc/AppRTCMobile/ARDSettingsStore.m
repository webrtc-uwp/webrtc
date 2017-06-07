/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "ARDSettingsStore.h"

static NSString *const kVideoResolutionKey = @"rtc_video_resolution_key";
static NSString *const kVideoCodecKey = @"rtc_video_codec_key";
static NSString *const kBitrateKey = @"rtc_max_bitrate_key";
static NSString *const kAudioOnlyKey = @"rtc_audio_only_key";
static NSString *const kCreateAecDumpKey = @"rtc_create_aec_dump_key";
static NSString *const kUseLevelControllerKey = @"rtc_use_level_controller_key";
static NSString *const kUseManualAudioConfigKey = @"rtc_use_manual_audio_config_key";

NS_ASSUME_NONNULL_BEGIN
@interface ARDSettingsStore () {
  NSUserDefaults *_storage;
}
@property(nonatomic, strong, readonly) NSUserDefaults *storage;
@end

@implementation ARDSettingsStore

- (NSUserDefaults *)storage {
  if (!_storage) {
    _storage = [NSUserDefaults standardUserDefaults];
  }
  return _storage;
}

- (NSString *)videoResolution {
  return [self.storage objectForKey:kVideoResolutionKey];
}

- (void)setVideoResolution:(NSString *)resolution {
  [self.storage setObject:resolution forKey:kVideoResolutionKey];
  [self.storage synchronize];
}

- (NSString *)videoCodec {
  return [self.storage objectForKey:kVideoCodecKey];
}

- (void)setVideoCodec:(NSString *)videoCodec {
  [self.storage setObject:videoCodec forKey:kVideoCodecKey];
  [self.storage synchronize];
}

- (nullable NSNumber *)maxBitrate {
  return [self.storage objectForKey:kBitrateKey];
}

- (void)setMaxBitrate:(nullable NSNumber *)value {
  [self.storage setObject:value forKey:kBitrateKey];
  [self.storage synchronize];
}

- (BOOL)hasSettingForAudioOnly {
  return [self hasPrimitiveSetting:kAudioOnlyKey];
}

- (BOOL)hasSettingForCreateAecDump {
  return [self hasPrimitiveSetting:kCreateAecDumpKey];
}

- (BOOL)hasSettingForUseLevelController {
  return [self hasPrimitiveSetting:kUseLevelControllerKey];
}

- (BOOL)hasSettingForUseManualAudioConfig {
  return [self hasPrimitiveSetting:kUseManualAudioConfigKey];
}

- (BOOL)audioOnly {
  return [self.storage boolForKey:kAudioOnlyKey];
}

- (void)setAudioOnly:(BOOL)audioOnly {
  [self.storage setBool:audioOnly forKey:kAudioOnlyKey];
  [self.storage synchronize];
}

- (BOOL)createAecDump {
  return [self.storage boolForKey:kCreateAecDumpKey];
}

- (void)setCreateAecDump:(BOOL)createAecDump {
  [self.storage setBool:createAecDump forKey:kCreateAecDumpKey];
  [self.storage synchronize];
}

- (BOOL)useLevelController {
  return [self.storage boolForKey:kUseLevelControllerKey];
}

- (void)setUseLevelController:(BOOL)useLevelController {
  [self.storage setBool:useLevelController forKey:kUseLevelControllerKey];
  [self.storage synchronize];
}

- (BOOL)useManualAudioConfig {
  return [self.storage boolForKey:kUseManualAudioConfigKey];
}

- (void)setUseManualAudioConfig:(BOOL)useManualAudioConfig {
  [self.storage setBool:useManualAudioConfig forKey:kUseManualAudioConfigKey];
  [self.storage synchronize];
}

#pragma mark -

- (BOOL)hasPrimitiveSetting:(NSString *)key {
  return [self.storage objectForKey:key] != nil;
}

@end
NS_ASSUME_NONNULL_END
