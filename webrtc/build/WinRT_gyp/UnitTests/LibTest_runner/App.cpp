
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

#include "webrtc/build/WinRT_gyp/UnitTests/LibTest_runner/common.h"

bool autoClose = false;

namespace LibTest_runner {
  ref class LibTestApp sealed : public Windows::UI::Xaml::Application {
  public:
    LibTestApp() {
    }

  private:
    Windows::UI::Xaml::Controls::TextBox^ outputTextBox_;
    Windows::UI::Xaml::Controls::ProgressRing^ progressRing_;

  protected:
    void OnLaunched(
      Windows::ApplicationModel::Activation::LaunchActivatedEventArgs^ e)
                                                                    override {
      auto layoutRoot = ref new Windows::UI::Xaml::Controls::Grid();
      layoutRoot->VerticalAlignment =
        Windows::UI::Xaml::VerticalAlignment::Center;
      layoutRoot->HorizontalAlignment =
        Windows::UI::Xaml::HorizontalAlignment::Center;

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
    }

    void RunAllTests() {
      SpWStringReporter_t spStringReporter(new CWStringReporter());

      // Update the UI to indicate test execution is in progress
      progressRing_->IsActive = true;

      // Run test cases in a separate thread not to block the UI thread
      // Pass the UI thread to continue using it after task execution
      auto ui = concurrency::task_continuation_context::use_current();
      concurrency::create_task([this, ui, spStringReporter]() {
        LibTest_runner::TestSolution::Instance().AddReporter(spStringReporter);
        LibTest_runner::TestSolution::Instance().AddReporter(SpXmlReporter_t(
          new CXmlReporter(ref new Platform::String(L"tests.xml"),
                           CXmlReporter::kAllTests)));
        LibTest_runner::TestSolution::Instance().Execute();
        LibTest_runner::TestSolution::Instance().GenerateReport();
      }).then([this, spStringReporter]() {
        // Update the UI
        if (spStringReporter->GetReport() != NULL) {
          outputTextBox_->Text = ref new Platform::String(
            (*spStringReporter->GetReport()).c_str());
          outputTextBox_->Text += L"Execution finished, will exit in 60s.\n";
        }

        progressRing_->IsActive = false;
        Sleep(60 * 1000);
        LibTestApp::Current->Exit();
      }, ui);
    }
  };
}  // namespace LibTest_runner

int __cdecl main(::Platform::Array<::Platform::String^>^ args) {
  (void)args;  // Unused parameter
  Windows::UI::Xaml::Application::Start(
    ref new Windows::UI::Xaml::ApplicationInitializationCallback(
      [](Windows::UI::Xaml::ApplicationInitializationCallbackParams^ p) {
    (void)p;  // Unused parameter
    auto app = ref new LibTest_runner::LibTestApp();
  }));

  return 0;
}
