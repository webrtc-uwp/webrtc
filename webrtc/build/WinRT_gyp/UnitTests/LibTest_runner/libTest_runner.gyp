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
      'target_name': 'libTest_runner',
      'type': 'executable',
      'dependencies': [
        '../../../../../third_party/libsrtp/libsrtp.gyp:libsrtp',
        '../../../../../third_party/libsrtp/libsrtp.gyp:rdbx_driver',
        '../../../../../third_party/libsrtp/libsrtp.gyp:srtp_driver',
        '../../../../../third_party/libsrtp/libsrtp.gyp:roc_driver',
        '../../../../../third_party/libsrtp/libsrtp.gyp:replay_driver',
        '../../../../../third_party/libsrtp/libsrtp.gyp:rtpw',
        '../../../../../third_party/libsrtp/libsrtp.gyp:srtp_test_cipher_driver',
        '../../../../../third_party/libsrtp/libsrtp.gyp:srtp_test_datatypes_driver',
        '../../../../../third_party/libsrtp/libsrtp.gyp:srtp_test_stat_driver',
        '../../../../../third_party/libsrtp/libsrtp.gyp:srtp_test_kernel_driver',
        '../../../../../third_party/libsrtp/libsrtp.gyp:srtp_test_aes_calc',
        '../../../../../third_party/libsrtp/libsrtp.gyp:srtp_test_rand_gen',
        '../../../../../third_party/libsrtp/libsrtp.gyp:srtp_test_env',
        '../../../../../third_party/libsrtp/libsrtp.gyp:srtp_test_sha1_driver',
        '../../../../../third_party/opus/opus.gyp:test_opus_encode',
        '../../../../../third_party/opus/opus.gyp:test_opus_api',
        '../../../../../third_party/opus/opus.gyp:test_opus_decode',
        '../../../../../third_party/opus/opus.gyp:test_opus_padding',
        #'../../../../../third_party/boringssl/boringssl_tests.gyp:boringssl_aead_test',
        '../../../../../third_party/boringssl/boringssl_tests.gyp:boringssl_base64_test',
        '../../../../../third_party/boringssl/boringssl_tests.gyp:boringssl_bio_test',
        '../../../../../third_party/boringssl/boringssl_tests.gyp:boringssl_bn_test',
        '../../../../../third_party/boringssl/boringssl_tests.gyp:boringssl_bytestring_test',
        '../../../../../third_party/boringssl/boringssl_tests.gyp:boringssl_constant_time_test',
        '../../../../../third_party/boringssl/boringssl_tests.gyp:boringssl_dh_test',
        '../../../../../third_party/boringssl/boringssl_tests.gyp:boringssl_digest_test',
        '../../../../../third_party/boringssl/boringssl_tests.gyp:boringssl_dsa_test',
        '../../../../../third_party/boringssl/boringssl_tests.gyp:boringssl_ec_test',
        '../../../../../third_party/boringssl/boringssl_tests.gyp:boringssl_ecdsa_test',
        '../../../../../third_party/boringssl/boringssl_tests.gyp:boringssl_err_test',
        '../../../../../third_party/boringssl/boringssl_tests.gyp:boringssl_gcm_test',
        '../../../../../third_party/boringssl/boringssl_tests.gyp:boringssl_hmac_test',
        '../../../../../third_party/boringssl/boringssl_tests.gyp:boringssl_lhash_test',
        '../../../../../third_party/boringssl/boringssl_tests.gyp:boringssl_rsa_test',
        '../../../../../third_party/boringssl/boringssl_tests.gyp:boringssl_pkcs7_test',
        '../../../../../third_party/boringssl/boringssl_tests.gyp:boringssl_pkcs12_test',
        '../../../../../third_party/boringssl/boringssl_tests.gyp:boringssl_example_mul',
        '../../../../../third_party/boringssl/boringssl_tests.gyp:boringssl_evp_test',
        '../../../../../third_party/boringssl/boringssl_tests.gyp:boringssl_ssl_test',
        '../../../../../third_party/boringssl/boringssl_tests.gyp:boringssl_pqueue_test',
        '../../../../../third_party/boringssl/boringssl_tests.gyp:boringssl_hkdf_test',
        '../../../../../third_party/boringssl/boringssl_tests.gyp:boringssl_pbkdf_test',
        '../../../../../third_party/boringssl/boringssl_tests.gyp:boringssl_thread_test',
        '../../../../modules/modules.gyp:rtp_player',
        '../../../../modules/modules.gyp:video_capture',
      ],
      'defines': [
         '_HAS_EXCEPTIONS=1',
      ],
      'include_dirs': [
        '.',
      ],
      'sources': [
        'App.cpp',
        'common.h',
        'Helpers/SafeSingleton.h',
        'Helpers/StdOutputRedirector.h',
        'Helpers/TestInserter.h',
        'TestSolution/TestBase.cpp',
        'TestSolution/TestBase.h',
        'TestSolution/TestSolution.cpp',
        'TestSolution/TestSolution.h',
        'TestSolution/TestsReporterBase.h',
        'TestSolution/WStringReporter.cpp',
        'TestSolution/WStringReporter.h',
        'TestSolution/XmlReporter.cpp',
        'TestSolution/XmlReporter.h',
        'TestSolution/TestSolutionProvider.h',
        'libSrtpTests/LibSrtpTestBase.cpp',
        'libSrtpTests/LibSrtpTestBase.h',
        'libSrtpTests/RdbxDriverTest.cpp',
        'libSrtpTests/RdbxDriverTest.h',
        'libSrtpTests/ReplayDriverTest.cpp',
        'libSrtpTests/ReplayDriverTest.h',
        'libSrtpTests/RocDriverTest.cpp',
        'libSrtpTests/RocDriverTest.h',
        'libSrtpTests/RtpwTest.cpp',
        'libSrtpTests/RtpwTest.h',
        'libSrtpTests/SrtpAesCalcTest.cpp',
        'libSrtpTests/SrtpAesCalcTest.h',
        'libSrtpTests/SrtpCipherDriverTest.cpp',
        'libSrtpTests/SrtpCipherDriverTest.h',
        'libSrtpTests/SrtpDatatypesDriverTest.cpp',
        'libSrtpTests/SrtpDatatypesDriverTest.h',
        'libSrtpTests/SrtpDriverTest.cpp',
        'libSrtpTests/SrtpDriverTest.h',
        'libSrtpTests/SrtpEnvTest.cpp',
        'libSrtpTests/SrtpEnvTest.h',
        'libSrtpTests/SrtpKernelDriverTest.cpp',
        'libSrtpTests/SrtpKernelDriverTest.h',
        'libSrtpTests/SrtpRandGenTest.cpp',
        'libSrtpTests/SrtpRandGenTest.h',
        'libSrtpTests/SrtpSha1DriverTest.cpp',
        'libSrtpTests/SrtpSha1DriverTest.h',
        'libSrtpTests/SrtpStatDriverTest.cpp',
        'libSrtpTests/SrtpStatDriverTest.h',
        'opusTests/OpusEncodeTest.cpp',
        'opusTests/OpusEncodeTest.h',
        'opusTests/OpusDecodeTest.cpp',
        'opusTests/OpusDecodeTest.h',
        'opusTests/OpusPaddingTest.cpp',
        'opusTests/OpusPaddingTest.h',
        'opusTests/OpusApiTest.cpp',
        'opusTests/OpusApiTest.h',
        'opusTests/OpusTestBase.cpp',
        'opusTests/OpusTestBase.h',
        'rtpPlayerTests/RtpPlayerTest.cpp',
        'rtpPlayerTests/RtpPlayerTest.h',
        'rtpPlayerTests/RtpPlayerTestBase.cpp',
        'rtpPlayerTests/RtpPlayerTestBase.h',
        'boringsslTests/SimpleTest.cpp',
        'boringsslTests/SimpleTest.h',
        'boringsslTests/BoringSslTestBase.cpp',
        'boringsslTests/BoringSslTestBase.h',
        'Logo.png',
        'SmallLogo.png',
        'StoreLogo.png',
        '..\gtest_runner\gtest_runner_TemporaryKey.pfx',
      ],
      'forcePackage': [
            'resources',
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
              ],
            }],
          ],
        }],
      ],
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)/libTest_runner_package',
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
            ['OS_RUNTIME=="winrt" and (winrt_platform=="win10" or winrt_platform=="win10_arm")', {
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
              ],
            }],
          ],
          'files':[
            'Logo.png',
            'SmallLogo.png',
            'StoreLogo.png',
            'resources'
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
            'resources',
          ],
        },
      ],
      'msvs_disabled_warnings': [
      ],
      'msvs_package_certificate': {
        'KeyFile': '..\gtest_runner\gtest_runner_TemporaryKey.pfx',
        'Thumbprint': 'E3AA95A6CD6D9DF6D0B7C68EBA246B558824F8C1',
      },
      'msvs_settings': {
        'VCCLCompilerTool': {
          # ExceptionHandling must match _HAS_EXCEPTIONS above.
          'ExceptionHandling': '1',
        },
        'VCLinkerTool': {
           'conditions': [
             ['OS_RUNTIME=="winrt" and (winrt_platform=="win10" or winrt_platform=="win10_arm")', {
                'AdditionalDependencies': [
                'WindowsApp.lib',
              ],
            }],
          ],
        },
      },
    },
    {
      'target_name': 'libTest_runner_appx',
      'product_name': 'libTest_runner',
      'product_extension': 'appx',
      'type': 'none',
      'dependencies': [
        'libTest_runner',
      ],
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)/libTest_runner_package',
          'files':[
            '<(PRODUCT_DIR)/libTest_runner.exe',
          ],
        },
      ],
      'appx': {
        'dep': '<(PRODUCT_DIR)/libTest_runner_package/libTest_runner.exe',
        'dir': '<(PRODUCT_DIR)/libTest_runner_package',
        'out': '<(PRODUCT_DIR)/libTest_runner.appx',
        'cert': '..\gtest_runner\gtest_runner_TemporaryKey.pfx'
      },
    },
  ],
}

