
// Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <collection.h>
#include <ppltasks.h>
#include <string>
#include "webrtc/test/test_suite.h"
#include "webrtc/base/win32.h"
#include "webrtc/base/ssladapter.h"
#include "webrtc/base/gunit.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/common_video/video_common_winrt.h"

static char stdout_buffer[1024 * 1024] = { 0 };

bool autoClose = false;


namespace gtest_runner {
  ref class GTestApp sealed : public Windows::UI::Xaml::Application {
  public:
    GTestApp() {
      progressTimer = ref new Windows::UI::Xaml::DispatcherTimer;
      progressTimer->Tick +=
        ref new Windows::Foundation::EventHandler<Object^>(this,
        &GTestApp::progressUpdate);
      Windows::Foundation::TimeSpan t;
      t.Duration = 10* 10*1000*1000;  // 10sec
      progressTimer->Interval = t;
    }

  private:
    Windows::UI::Xaml::Controls::TextBox^ outputTextBox_;
    Windows::UI::Xaml::Controls::ProgressRing^ progressRing_;
    Windows::UI::Xaml::DispatcherTimer^ progressTimer;

  protected:
    virtual void OnLaunched(
      Windows::ApplicationModel::Activation::LaunchActivatedEventArgs^ e)
                                                                     override {
      webrtc::VideoCommonWinRT::SetCoreDispatcher(Windows::UI::Xaml::Window::Current->Dispatcher);
	  
      auto layoutRoot = ref new Windows::UI::Xaml::Controls::Grid();
      layoutRoot->VerticalAlignment = Windows::UI::Xaml::VerticalAlignment::Center;
      layoutRoot->HorizontalAlignment = Windows::UI::Xaml::HorizontalAlignment::Center;

      outputTextBox_ = ref new Windows::UI::Xaml::Controls::TextBox();
      outputTextBox_->Width = 640;
      outputTextBox_->Height = 480;
      outputTextBox_->AcceptsReturn = true;
      outputTextBox_->PlaceholderText = "Test outputs appears here!";
      layoutRoot->Children->Append(outputTextBox_);

      progressRing_ = ref new Windows::UI::Xaml::Controls::ProgressRing();
      progressRing_->Width = 50;
      progressRing_->Height = 50;
      layoutRoot->Children->Append(progressRing_);

      Windows::UI::Xaml::Window::Current->Content = layoutRoot;
      Windows::UI::Xaml::Window::Current->Activate();
      RunAllTests();

      progressTimer->Start();
    }

    void RunAllTests() {
      testing::FLAGS_gtest_output = "xml";

      // Update the UI to indicate test execution is in progress
      progressRing_->IsActive = true;
      outputTextBox_->PlaceholderText = "Executing test cases. Please wait...";

      // Capture stdout
      setvbuf(stdout, stdout_buffer, _IOFBF, sizeof(stdout_buffer));

      // Initialize SSL which are used by several tests.
      rtc::InitializeSSL();

      // Run test cases in a separate thread not to block the UI thread
      // Pass the UI thread to continue using it after task execution
      auto ui = concurrency::task_continuation_context::use_current();
      concurrency::create_task([this, ui]() {
        char* argv[] = { "." };
        webrtc::test::TestSuite test_suite(1, argv);
        test_suite.Run();
      }).then([this]() {
        // Cleanup SSL initialized
        rtc::CleanupSSL();

        // Update the UI
        outputTextBox_->Text = ref new Platform::String(rtc::ToUtf16(stdout_buffer,
          strlen(stdout_buffer)).data());
        progressRing_->IsActive = false;

        // Exit the app
        GTestApp::Current->Exit();
      }, ui);
    }

    void progressUpdate(Platform::Object^ sender, Platform::Object^ e) {
      if (!::testing::UnitTest::GetInstance()->current_test_case())
        return;

      std::ostringstream stringStream;

      SYSTEMTIME st;
      GetLocalTime(&st);

      std::string currentTestCase =
        ::testing::UnitTest::GetInstance()->current_test_case()->name();
      // FixMe:
      // Test count is not thread safe. fortunately
      // We will add check it when we fixed the threading issue.
      // int total = ::testing::UnitTest::GetInstance()->test_to_run_count();
      // int finished =
      //  ::testing::UnitTest::GetInstance()->successful_test_count() +
      //  ::testing::UnitTest::GetInstance()->failed_test_count();

      stringStream << "Executing test cases. Please wait...\n"
        << "Current Test case:" << currentTestCase << "\n"
        // << finished << "/" << total << "test suite finished" << "\n"
        << "\n" << "Last Status updated at "
        << st.wHour << ":" << st.wMinute << ":" << st.wSecond;

      std::string s_str = stringStream.str();
      std::wstring wid_str = std::wstring(s_str.begin(), s_str.end());

      outputTextBox_->Text = ref new Platform::String(wid_str.c_str());
    }
  };
}  // namespace gtest_runner

int __cdecl main(::Platform::Array<::Platform::String^>^ args) {
  (void)args;  // Unused parameter
  Windows::UI::Xaml::Application::Start(
    ref new Windows::UI::Xaml::ApplicationInitializationCallback(
    [](Windows::UI::Xaml::ApplicationInitializationCallbackParams^ p) {
    (void)p;  // Unused parameter
    auto app = ref new gtest_runner::GTestApp();
  }));

  return 0;
}
