# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': ['../common.gypi',],
  'variables': {

  },
  'targets': [
    {
      'target_name': 'winuwp_all',
      'type': 'none',
      'dependencies': [
        'UnitTests/LibTest_runner/libTest_runner.gyp:*',
        'UnitTests/gtest_runner/gtest_runner.gyp:*',
        'api/api.gyp:*',
        'stats/stats.gyp:*',
        '<(webrtc_root)/../third_party/winuwp_h264/winuwp_h264.gyp:*',
      ],
    },
  ],
}
