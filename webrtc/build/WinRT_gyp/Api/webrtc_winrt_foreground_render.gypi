# Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'targets': [
    {
      'target_name': 'webrtc_winrt_foreground_render',
      'type': 'shared_library',
      'include_dirs': [
        '.',
      ],
      'msvs_disabled_warnings': [
        '4458',  # local members hides previously defined memebers or function members or class members
      ],
      'defines': [
         '_HAS_EXCEPTIONS=1',
         '_WINRT_DLL',
        'NTDDI_VERSION=NTDDI_WIN10',
      ],
      'sources': [
        'SwapChainPanelSource.h',
        'SwapChainPanelSource.cc',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'UseLibraryDependencyInputs': "true",
          'AdditionalOptions': [
            '/WINMD',
          ],
          'WindowsMetadataFile':'$(OutDir)webrtc_winrt_foreground_render.winmd',
          'AdditionalDependencies': [
            'WindowsApp.lib',
          ],
        },
      },
    },
  ],
}
