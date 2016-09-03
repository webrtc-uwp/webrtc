# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': ['../../../common.gypi',],
  'variables': {

  },
  'targets': [
    {
      'target_name': 'gtest_runner',
      'type': 'executable',
      'dependencies': [
        '<(webrtc_root)/webrtc.gyp:rtc_media_unittests',
        '<(webrtc_root)/webrtc.gyp:rtc_pc_unittests',
        '<(webrtc_root)/api/api_tests.gyp:peerconnection_unittests',
        '<(webrtc_root)/common_audio/common_audio.gyp:common_audio_unittests',
        '<(webrtc_root)/common_video/common_video_unittests.gyp:common_video_unittests',
        '<(webrtc_root)/modules/modules.gyp:modules_tests',
        '<(webrtc_root)/modules/modules.gyp:modules_unittests',
        '<(webrtc_root)/modules/modules.gyp:video_capture_tests',
        '<(webrtc_root)/system_wrappers/system_wrappers_tests.gyp:system_wrappers_unittests',
        '<(webrtc_root)/test/test.gyp:test_support',
        '<(webrtc_root)/test/test.gyp:test_support_main',
        '<(webrtc_root)/test/test.gyp:test_support_unittests',
        '<(webrtc_root)/voice_engine/voice_engine.gyp:voice_engine_unittests',
        '<(webrtc_root)/webrtc.gyp:rtc_unittests',
        '../../../../../third_party/libyuv/libyuv_test.gyp:libyuv_unittest',
        '../../../../test/test.gyp:test_common',
      ],
      'defines': [
        'GTEST_RELATIVE_PATH',
        '_HAS_EXCEPTIONS=1',
      ],
      'include_dirs': [
        '../../../../../testing/gtest/include',
      ],
      'sources': [
        'App.cpp',
        'gtest_runner_TemporaryKey.pfx',
        'Logo.png',
        'SmallLogo.png',
        'StoreLogo.png',
      ],
      'forcePackage': [
            '../../../../../resources/',
            '../../../../../data/',
            '../../../../../resources/media/',
            '../../../../../chromium/src/third_party/libjingle/source/talk/media/testdata/',
      ],
      'conditions': [
        ['OS_RUNTIME=="winrt" and winrt_platform=="win_phone"', {
          'sources': [
            'Package.phone.appxmanifest',
            'Logo71x71.png',
            'Logo44x44.png',
            'SplashScreen480x800.png',
          ],
        }],
        ['OS_RUNTIME=="winrt" and winrt_platform=="win"', {
          'sources': [
            'Package.appxmanifest',
            'SplashScreen.png',
          ],
        }],
        ['OS_RUNTIME=="winrt" and (winrt_platform=="win10" or winrt_platform=="win10_arm")', {
          'sources': [
            'Package.Win10.appxmanifest',
            'SplashScreen.png',
          ],
          'conditions': [
            ['target_arch=="x64"', {
              'sources': [
                'Generated Manifest Win10\\x64\AppxManifest.xml',
              ],
            },{
              'sources': [
                'Generated Manifest Win10\\AppxManifest.xml',
              ]
            }],
          ]
        }],
      ],
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)/gtest_runner_package',
          'conditions': [
            ['OS_RUNTIME=="winrt" and winrt_platform=="win_phone"', {
              'files': [	
                 'Generated Manifest Phone\AppxManifest.xml',								
                 'Logo71x71.png',
                 'Logo44x44.png',
                 'SplashScreen480x800.png',
              ],
            }],
            ['OS_RUNTIME=="winrt" and winrt_platform=="win"', {
              'files': [
                'Generated Manifest\AppxManifest.xml',								
                'SplashScreen.png',
              ],
            }],
            ['OS_RUNTIME=="winrt" and winrt_platform=="win10"', {
              'files': [
                'SplashScreen.png',
              ],
              'conditions': [
                ['target_arch=="x64"', {
                  'files': [
                    'Generated Manifest Win10\\x64\AppxManifest.xml',
                  ],
                },{
                  'files': [
                    'Generated Manifest Win10\\AppxManifest.xml',
                  ],
                }],
              ]
            }],
          ],
          'files':[
            'Logo.png',
            'SmallLogo.png',
            'StoreLogo.png',
            '../../../../../data/',
            '../../../../../resources/',
            '../../../../../resources/media/',
            '../../../../../chromium/src/third_party/libjingle/source/talk/media/testdata/',
          ],
        },
        # Hack for MSVS to copy to the Appx folder
        {
          'destination': '<(PRODUCT_DIR)/AppX',
          'conditions': [
              ['OS_RUNTIME=="winrt" and winrt_platform=="win_phone"', {
                'files': [	
                   'Logo71x71.png',
                   'Logo44x44.png',	
                   'SplashScreen480x800.png',							
                ],
            }],
             ['OS_RUNTIME=="winrt" and winrt_platform!="win_phone"', {
                'files': [
                  'SplashScreen.png',
                ],
            }],
          ],
          'files':[
            'Logo.png',
            'SmallLogo.png',
            'StoreLogo.png',
            '../../../../../data/',
            '../../../../../resources/',
            '../../../../../resources/media/',
            '../../../../../chromium/src/third_party/libjingle/source/talk/media/testdata/',
          ],
        },
      ],
      'msvs_disabled_warnings': [
        4453,  # A '[WebHostHidden]' type should not be used on the published surface of a public type that is not '[WebHostHidden]'
      ],
      'msvs_package_certificate': {
        'KeyFile': 'gtest_runner_TemporaryKey.pfx',
        'Thumbprint': 'E3AA95A6CD6D9DF6D0B7C68EBA246B558824F8C1',
      },
      'msvs_settings': {
        'VCCLCompilerTool': {
          # ExceptionHandling must match _HAS_EXCEPTIONS above.
          'ExceptionHandling': '1',
        },
        'VCLinkerTool': {
          'UseLibraryDependencyInputs': "true",
          'AdditionalDependencies': [
           'ws2_32.lib',
          ],
          # 2 == /SUBSYSTEM:WINDOWS
          'SubSystem': '2',
          'conditions': [
            ['OS_RUNTIME=="winrt" and (winrt_platform=="win_phone" or winrt_platform=="win10_arm")', {
              'AdditionalOptions': [
                # Fixes linking for assembler opus source files 
                '<(PRODUCT_DIR)/obj/opus/celt_pitch_xcorr_arm.obj',
                '<(SHARED_INTERMEDIATE_DIR)/third_party/libvpx_new/*.obj',
                '../../../../../third_party/libyuv/source/*.obj',
                '<(PRODUCT_DIR)/obj/openmax_dl_armv7/*.obj',
                '<(PRODUCT_DIR)/obj/openmax_dl_neon/*.obj',
                '<(PRODUCT_DIR)/obj/common_audio/spl_sqrt_floor_arm.obj',
                '<(PRODUCT_DIR)/obj/common_audio/filter_ar_fast_q12_armv7.obj',
                '<(PRODUCT_DIR)/obj/common_audio/complex_bit_reverse_arm.obj',
                '<(PRODUCT_DIR)/obj/isac_fix/lattice_armv7.obj',
                '<(PRODUCT_DIR)/obj/isac_fix/pitch_filter_armv6.obj',
              ],
            }],
            ['OS_RUNTIME=="winrt" and (winrt_platform=="win10" or winrt_platform=="win10_arm")', {
              'AdditionalDependencies': [
                'ws2_32.lib',
                'WindowsApp.lib'
              ],
            }],
          ],
        },
      },
    },
    {
      'target_name': 'gtest_runner_appx',
      'type': 'none',
      'dependencies': [
        'gtest_runner',
      ],
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)/gtest_runner_package',
          'files':[
            '<(PRODUCT_DIR)/gtest_runner.exe',
          ],
        },
      ],
      'appx': {
        'dep': '<(PRODUCT_DIR)/gtest_runner_package/gtest_runner.exe',
        'dir': '<(PRODUCT_DIR)/gtest_runner_package',
        'cert': 'gtest_runner_TemporaryKey.pfx',
        'out': '<(PRODUCT_DIR)/gtest_runner.appx',
      },
    },
  ],
}
