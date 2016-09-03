// Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "webrtc/build/WinRT_gyp/Api/SwapChainPanelSource.h"

using webrtc_winrt_foreground_render::SwapChainPanelSource;
using Platform::COMException;
using Windows::ApplicationModel::Background::BackgroundTaskRegistration;
using Windows::ApplicationModel::Background::BackgroundTaskCompletedEventArgs;
using Windows::Foundation::EventRegistrationToken;
using Windows::UI::Core::DispatchedHandler;
using Windows::UI::Core::CoreDispatcherPriority;

SwapChainPanelSource::SwapChainPanelSource() : _swapChain(nullptr),
_currentSwapChainHandle(nullptr) {
}

SwapChainPanelSource::~SwapChainPanelSource() {
  StopSource();
}

void SwapChainPanelSource::StartSource(
  Windows::UI::Xaml::Controls::SwapChainPanel^ swapChainPanel) {
  StopSource();
  _swapChain = swapChainPanel;
  // Shutdown media stream source when SwapChain control is destroyed (Unloaded)
  _swapChain->Unloaded += ref new Windows::UI::Xaml::RoutedEventHandler(
    this, &webrtc_winrt_foreground_render::SwapChainPanelSource::OnUnloaded);
  HRESULT hr;
  if ((FAILED(hr = reinterpret_cast<IUnknown*>(swapChainPanel)->
    QueryInterface(IID_PPV_ARGS(&_nativeSwapChain)))) ||
    (!_nativeSwapChain)) {
    throw ref new COMException(hr);
  }
}

void SwapChainPanelSource::StopSource() {
  if (_nativeSwapChain != nullptr) {
    _swapChain->Dispatcher->RunAsync(CoreDispatcherPriority::Normal,
      ref new DispatchedHandler([this]() {
      _nativeSwapChain->SetSwapChainHandle(nullptr);
      if (_currentSwapChainHandle != nullptr) {
        CloseHandle(_currentSwapChainHandle);
      }
    }));
  }
  else if (_currentSwapChainHandle != nullptr) {
    CloseHandle(_currentSwapChainHandle);
  }
}

void SwapChainPanelSource::UpdateFormat(uint32 width, uint32 height,
  uintptr_t swapChainHandle) {
  _swapChain->Dispatcher->RunAsync(CoreDispatcherPriority::Normal,
    ref new DispatchedHandler([this, swapChainHandle]() {
    if (_nativeSwapChain != nullptr) {
      _nativeSwapChain->SetSwapChainHandle((HANDLE)swapChainHandle);
    }
    if (_currentSwapChainHandle != nullptr) {
      CloseHandle(_currentSwapChainHandle);
    }
    _currentSwapChainHandle = (HANDLE)swapChainHandle;
  }));
}

void SwapChainPanelSource::OnRegistrationCompleted(
  BackgroundTaskRegistration ^sender, BackgroundTaskCompletedEventArgs ^args) {
  args->CheckResult();
}

void SwapChainPanelSource::OnUnloaded(Platform::Object ^sender,
  Windows::UI::Xaml::RoutedEventArgs ^e) {
  StopSource();
}
