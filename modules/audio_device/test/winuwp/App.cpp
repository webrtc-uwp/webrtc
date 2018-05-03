/*
*  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
*
*  Use of this source code is governed by a BSD-style license
*  that can be found in the LICENSE file in the root of the source
*  tree. An additional intellectual property rights grant can be found
*  in the file PATENTS.  All contributing project authors may
*  be found in the AUTHORS file in the root of the source tree.
*/

#include <collection.h>
#include <ppltasks.h>
#include <string>

#include "WinUWPTestManager.h"
#include "webrtc/modules/audio_device/audio_device_config.h"
#include "webrtc/modules/audio_device/audio_device_impl.h"

using namespace Platform;
using namespace concurrency;
using namespace Windows::ApplicationModel::Activation;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Media;

static char stdout_buffer[1024 * 1024] = { 0 };

bool autoClose = false;

namespace audio_device_test_winuwp {
  ref class AudioDeviceTestWinUWP sealed : public Windows::UI::Xaml::Application {
  public:
    AudioDeviceTestWinUWP() {
    }
  private:
    ProgressRing^ progressRing_;
    Button^ skipButton_;
    Button^ testDeviceEnumerationButton_;
    Button^ testDeviceSelectionButton_;
    Button^ testAudioTransportButton_;
    Button^ testLoopbackButton_;
    Button^ testSpeakerVolumeButton_;
    Button^ testMicrophoneVolumeButton_;
    Button^ testSpeakerMuteButton_;
    Button^ testMicrophoneMuteButton_;
    Button^ testMicrophoneAGCButton_;
    Button^ testDeviceRemovalButton_;
    Button^ testExtraButton_;

  protected:
    void OnLaunched(Windows::ApplicationModel::Activation::LaunchActivatedEventArgs^ e) override {
      auto layoutRoot = ref new Grid();
      layoutRoot->VerticalAlignment = VerticalAlignment::Top;
      layoutRoot->HorizontalAlignment = HorizontalAlignment::Left;

      auto containerStack = ref new StackPanel();
      containerStack->Orientation = Orientation::Horizontal;

      auto buttonStack = ref new StackPanel();
      buttonStack->Margin =
        Windows::UI::Xaml::Thickness(150.0, 100.0, 0.0, 0.0);
      buttonStack->Orientation = Orientation::Vertical;
      buttonStack->HorizontalAlignment = HorizontalAlignment::Left;

      testDeviceEnumerationButton_ = ref new Button();
      testDeviceEnumerationButton_->Margin = Windows::UI::Xaml::Thickness(0.0, 20.0, 0.0, 0.0);
      testDeviceEnumerationButton_->Width = 200.0;
      testDeviceEnumerationButton_->Content = "TestDeviceEnumeration";
      testDeviceEnumerationButton_->Click += ref new RoutedEventHandler(this, &audio_device_test_winuwp::AudioDeviceTestWinUWP::testDeviceEnumerationButton_Click);
      buttonStack->Children->Append(testDeviceEnumerationButton_);

      testDeviceSelectionButton_ = ref new Button();
      testDeviceSelectionButton_->Margin = Windows::UI::Xaml::Thickness(0.0, 20.0, 0.0, 0.0);
      testDeviceSelectionButton_->Width = 200.0;
      testDeviceSelectionButton_->Content = "TestDeviceSelection";
      testDeviceSelectionButton_->Click += ref new RoutedEventHandler(this, &audio_device_test_winuwp::AudioDeviceTestWinUWP::testDeviceSelectionButton_Click);
      buttonStack->Children->Append(testDeviceSelectionButton_);

      testAudioTransportButton_ = ref new Button();
      testAudioTransportButton_->Margin = Windows::UI::Xaml::Thickness(0.0, 20.0, 0.0, 0.0);
      testAudioTransportButton_->Width = 200.0;
      testAudioTransportButton_->Content = "TestAudioTransport";
      testAudioTransportButton_->Click += ref new RoutedEventHandler(this, &audio_device_test_winuwp::AudioDeviceTestWinUWP::testAudioTransportButton_Click);
      buttonStack->Children->Append(testAudioTransportButton_);

      testLoopbackButton_ = ref new Button();
      testLoopbackButton_->Margin = Windows::UI::Xaml::Thickness(0.0, 20.0, 0.0, 0.0);
      testLoopbackButton_->Width = 200.0;
      testLoopbackButton_->Content = "TestLoopback";
      testLoopbackButton_->Click += ref new RoutedEventHandler(this, &audio_device_test_winuwp::AudioDeviceTestWinUWP::testLoopbackButton_Click);
      buttonStack->Children->Append(testLoopbackButton_);

      testSpeakerVolumeButton_ = ref new Button();
      testSpeakerVolumeButton_->Margin = Windows::UI::Xaml::Thickness(0.0, 20.0, 0.0, 0.0);
      testSpeakerVolumeButton_->Width = 200.0;
      testSpeakerVolumeButton_->Content = "TestSpeakerVolume";
      testSpeakerVolumeButton_->Click += ref new RoutedEventHandler(this, &audio_device_test_winuwp::AudioDeviceTestWinUWP::testSpeakerVolumeButton_Click);
      buttonStack->Children->Append(testSpeakerVolumeButton_);

      testMicrophoneVolumeButton_ = ref new Button();
      testMicrophoneVolumeButton_->Margin = Windows::UI::Xaml::Thickness(0.0, 20.0, 0.0, 0.0);
      testMicrophoneVolumeButton_->Width = 200.0;
      testMicrophoneVolumeButton_->Content = "TestMicrophoneVolume";
      testMicrophoneVolumeButton_->Click += ref new RoutedEventHandler(this, &audio_device_test_winuwp::AudioDeviceTestWinUWP::testMicrophoneVolumeButton_Click);
      buttonStack->Children->Append(testMicrophoneVolumeButton_);

      testSpeakerMuteButton_ = ref new Button();
      testSpeakerMuteButton_->Margin = Windows::UI::Xaml::Thickness(0.0, 20.0, 0.0, 0.0);
      testSpeakerMuteButton_->Width = 200.0;
      testSpeakerMuteButton_->Content = "TestSpeakerMute";
      testSpeakerMuteButton_->Click += ref new RoutedEventHandler(this, &audio_device_test_winuwp::AudioDeviceTestWinUWP::testSpeakerMuteButton_Click);
      buttonStack->Children->Append(testSpeakerMuteButton_);

      testMicrophoneMuteButton_ = ref new Button();
      testMicrophoneMuteButton_->HorizontalAlignment = HorizontalAlignment::Left;
      testMicrophoneMuteButton_->VerticalAlignment = VerticalAlignment::Top;
      testMicrophoneMuteButton_->Margin = Windows::UI::Xaml::Thickness(0.0, 20.0, 0.0, 0.0);
      testMicrophoneMuteButton_->Width = 200.0;
      testMicrophoneMuteButton_->Content = "TestMicrophoneMute";
      testMicrophoneMuteButton_->Click += ref new RoutedEventHandler(this, &audio_device_test_winuwp::AudioDeviceTestWinUWP::testMicrophoneMuteButton_Click);
      buttonStack->Children->Append(testMicrophoneMuteButton_);

      testMicrophoneAGCButton_ = ref new Button();
      testMicrophoneAGCButton_->Margin = Windows::UI::Xaml::Thickness(0.0, 20.0, 0.0, 0.0);
      testMicrophoneAGCButton_->Width = 200.0;
      testMicrophoneAGCButton_->Content = "TestMicrophoneAGC";
      testMicrophoneAGCButton_->Click += ref new RoutedEventHandler(this, &audio_device_test_winuwp::AudioDeviceTestWinUWP::testMicrophoneAGCButton_Click);
      buttonStack->Children->Append(testMicrophoneAGCButton_);

      testDeviceRemovalButton_ = ref new Button();
      testDeviceRemovalButton_->Margin = Windows::UI::Xaml::Thickness(0.0, 20.0, 0.0, 0.0);
      testDeviceRemovalButton_->Width = 200.0;
      testDeviceRemovalButton_->Content = "TestDeviceRemoval";
      testDeviceRemovalButton_->Click += ref new RoutedEventHandler(this, &audio_device_test_winuwp::AudioDeviceTestWinUWP::testDeviceRemovalButton_Click);
      buttonStack->Children->Append(testDeviceRemovalButton_);

      testExtraButton_ = ref new Button();
      testExtraButton_->Margin = Windows::UI::Xaml::Thickness(0.0, 20.0, 0.0, 0.0);
      testExtraButton_->Width = 200.0;
      testExtraButton_->Content = "TestExtra";
      testExtraButton_->Click += ref new RoutedEventHandler(this, &audio_device_test_winuwp::AudioDeviceTestWinUWP::testExtraButton_Click);
      buttonStack->Children->Append(testExtraButton_);

      skipButton_ = ref new Button();
      skipButton_->VerticalAlignment = VerticalAlignment::Center;
      skipButton_->HorizontalAlignment = HorizontalAlignment::Center;
      skipButton_->Margin = Windows::UI::Xaml::Thickness(200.0, 100.0, 0.0, 0.0);
      skipButton_->Width = 200;
      skipButton_->Height = 60;
      skipButton_->Content = "Skip";
      skipButton_->Click += ref new RoutedEventHandler(this, &audio_device_test_winuwp::AudioDeviceTestWinUWP::skipButton_Click);

      containerStack->Children->Append(buttonStack);
      containerStack->Children->Append(skipButton_);
      layoutRoot->Children->Append(containerStack);

      progressRing_ = ref new ProgressRing();
      progressRing_->Width = 50;
      progressRing_->Height = 50;
      layoutRoot->Children->Append(progressRing_);

      Window::Current->Content = layoutRoot;
      Window::Current->Activate();
    }

    void skipButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e) {
      WinUWPTestManager::userSignalToContinue();
    }

    void testDeviceEnumerationButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e) {
      TestDeviceEnumerationAsync();
    }

    Windows::Foundation::IAsyncAction^ TestDeviceEnumerationAsync() {
      return create_async([this]
      {
        WinUWPTestManager *mManager = new WinUWPTestManager();
        mManager->Init();
        mManager->TestDeviceEnumeration();
      });
    }

    void testDeviceSelectionButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e) {
       TestDeviceSelectionAsync();
    }

    Windows::Foundation::IAsyncAction^ TestDeviceSelectionAsync() {
      return create_async([this]
      {
        WinUWPTestManager *mManager = new WinUWPTestManager();
        mManager->Init();
        mManager->TestDeviceSelection();
      });
    }

    void testAudioTransportButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e) {
      TestTransportAsync();
    }

    Windows::Foundation::IAsyncAction^ TestTransportAsync() {
      return create_async([this]
      {
        WinUWPTestManager *mManager = new WinUWPTestManager();
        mManager->Init();
        mManager->TestAudioTransport();
      });
    }

    void testLoopbackButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e) {
      TestLoopBackAsync();
    }

    Windows::Foundation::IAsyncAction^ TestLoopBackAsync(){
      return create_async([this]
      {
        WinUWPTestManager *mManager = new WinUWPTestManager();
        mManager->Init();
        mManager->TestLoopback();
      });
    }
    
    void testSpeakerVolumeButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e) {
      TestSpeakerVolumeAsync();
    }
    
    Windows::Foundation::IAsyncAction^ TestSpeakerVolumeAsync()
    {
      return create_async([this]
      {
        WinUWPTestManager *mManager = new WinUWPTestManager();
        mManager->Init();
        mManager->TestSpeakerVolume();
      });
    }

    void testMicrophoneVolumeButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e) {
      TestMicrophoneVolumeAsync();
    }

    Windows::Foundation::IAsyncAction^ TestMicrophoneVolumeAsync()
    {
      return create_async([this]
      {
        WinUWPTestManager *mManager = new WinUWPTestManager();
        mManager->Init();
        mManager->TestMicrophoneVolume();
      });
    }


    void testSpeakerMuteButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e) {
      TestSpeakerMuteAsync();
    }

    Windows::Foundation::IAsyncAction^ TestSpeakerMuteAsync()
    {
      return create_async([this]
      {
        WinUWPTestManager *mManager = new WinUWPTestManager();
        mManager->Init();
        mManager->TestSpeakerMute();
      });
    }


    void testMicrophoneMuteButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e) {
      TestMicrophoneMuteAsync();
    }

    Windows::Foundation::IAsyncAction^ TestMicrophoneMuteAsync()
    {
      return create_async([this]
      {
        WinUWPTestManager *mManager = new WinUWPTestManager();
        mManager->Init();
        mManager->TestMicrophoneMute();
      });
    }


    void testMicrophoneAGCButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e) {
      TestMicrophoneAGCAsync();
    }

    Windows::Foundation::IAsyncAction^ TestMicrophoneAGCAsync()
    {
      return create_async([this]
      {
        WinUWPTestManager *mManager = new WinUWPTestManager();
        mManager->Init();
        mManager->TestMicrophoneAGC();
      });
    }

    void testDeviceRemovalButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e) {
      TestDeviceRemovalAsync();
    }

    Windows::Foundation::IAsyncAction^ TestDeviceRemovalAsync()
    {
      return create_async([this]
      {
        WinUWPTestManager *mManager = new WinUWPTestManager();
        mManager->Init();
        mManager->TestDeviceRemoval();
      });
    }

    void testExtraButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e) {
      TestExtraAsync();
    }

    Windows::Foundation::IAsyncAction^ TestExtraAsync()
    {
      return create_async([this]
      {
        WinUWPTestManager *mManager = new WinUWPTestManager();
        mManager->Init();
        mManager->TestExtra();
      });
    }
  };
}  // namespace audio_device_test_winuwp

int __cdecl main(::Platform::Array<::Platform::String^>^ args) {
  (void)args;  // Unused parameter
  Windows::UI::Xaml::Application::Start(
    ref new Windows::UI::Xaml::ApplicationInitializationCallback(
    [](Windows::UI::Xaml::ApplicationInitializationCallbackParams^ p) {
    (void)p;  // Unused parameter
    auto app = ref new audio_device_test_winuwp::AudioDeviceTestWinUWP();
  }));

  return 0;
}
