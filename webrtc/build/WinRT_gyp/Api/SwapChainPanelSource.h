// Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef WEBRTC_BUILD_WINRT_GYP_API_SWAPCHAINPANELSOURCE_H_
#define WEBRTC_BUILD_WINRT_GYP_API_SWAPCHAINPANELSOURCE_H_

#include <Windows.h>
#include <collection.h>
#include <ppltasks.h>
#include <Windows.ui.xaml.media.dxinterop.h>

namespace webrtc_winrt_foreground_render {
  /// <summary>
  /// Delegate for triggering a COM error on the SwapChainPanel
  /// control.
  /// </summary>
  public delegate void ErrorDelegate(uint32 hr);

  /// <summary>
  /// SwapChainPanelSource object creates and maintains a
  /// connection to the background renderer.
  /// </summary>
  [Windows::Foundation::Metadata::WebHostHidden]
  public ref class SwapChainPanelSource sealed {
  public:
    SwapChainPanelSource();
    virtual ~SwapChainPanelSource();

    /// <summary>
    /// StartSource attaches a swap chain panel control to a background renderer and starts the video.
    /// </summary>
    /// <param name="swapChainPanel">SwapChainPanel control object that will receive video frames</param>
    void StartSource(Windows::UI::Xaml::Controls::SwapChainPanel^
      swapChainPanel);

    /// <summary>
    /// Stops the background renderer, detaches foreground 
    /// control from background, and cleans up resources.
    /// </summary>
    void StopSource();

    /// <summary>
    /// Called when the dimensions of the source video changes.
    /// </summary>
    /// <param name="width">New width of video frames</param>
    /// <param name="height">New height of video frames</param>
    /// <param name="swapChainHandle">Foreground copy of background swap chain handle</param>
    void UpdateFormat(uint32 width, uint32 height, uintptr_t swapChainHandle);

    /// <summary>
    /// Error event triggered when an error is detected on the foreground
    /// SwapChainPanel control.
    /// </summary>
    event ErrorDelegate^ Error;
  private:
    void OnRegistrationCompleted(
      Windows::ApplicationModel::Background::BackgroundTaskRegistration^
      sender,
      Windows::ApplicationModel::Background::BackgroundTaskCompletedEventArgs^
      args);
    void OnUnloaded(Platform::Object ^sender,
      Windows::UI::Xaml::RoutedEventArgs ^e);

    Windows::UI::Xaml::Controls::SwapChainPanel^ _swapChain;
    Microsoft::WRL::ComPtr<ISwapChainPanelNative2> _nativeSwapChain;
    HANDLE _currentSwapChainHandle;
  };
}  // namespace webrtc_winrt_foreground_render

#endif  // WEBRTC_BUILD_WINRT_GYP_API_SWAPCHAINPANELSOURCE_H_
