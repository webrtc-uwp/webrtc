/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <fstream>
#include <memory>
#include <string>

#include "logging/rtc_event_log/output/rtc_event_log_output_file.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/random.h"
#include "system_wrappers/include/clock.h"
#include "test/gtest.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {

class RtcEventLogOutputFileTest : public ::testing::Test {
 public:
  RtcEventLogOutputFileTest() : output_file_name_(GetOutputFilePath()) {
    // Ensure no leftovers from previous runs, which might not have terminated
    // in an orderly fashion.
    remove(output_file_name_.c_str());
  }

  ~RtcEventLogOutputFileTest() override {
    remove(output_file_name_.c_str());
  }

 protected:
  std::string GetOutputFilePath() const {
    auto test_info = ::testing::UnitTest::GetInstance()->current_test_info();
    return test::OutputPath() + test_info->test_case_name() + test_info->name();
  }

  const std::string output_file_name_;
};

TEST_F(RtcEventLogOutputFileTest, UnlimitedOutputFile) {
  // Random input to increase confidence that tests don't pass because of old
  // temporary files that failed to be deleted and/or overwritten.
  Random prng(Clock::GetRealTimeClock()->TimeInMicroseconds());

  const std::string temp_filename = GetOutputFilePath();

  std::stringstream output_stringstream;
  for (size_t i = 0; i < 1000; i++) {
    output_stringstream << std::to_string(prng.Rand(0, 9));
  }
  std::string output_str = output_stringstream.str();

  auto output_file = rtc::MakeUnique<RtcEventLogOutputFile>(temp_filename);
  output_file->Write(output_str);
  output_file.reset();  // Closing the file flushes the buffer to disk.

  std::ifstream file(temp_filename, std::ios_base::in | std::ios_base::binary);
  ASSERT_TRUE(file.good());
  ASSERT_TRUE(file.is_open());
  std::string file_str((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());

  EXPECT_EQ(output_str, file_str);
}

TEST_F(RtcEventLogOutputFileTest, LimitedOutputFile) {
  constexpr size_t file_size_limit = 50;

  // Random input to increase confidence that tests don't pass because of old
  // temporary files that failed to be deleted and/or overwritten.
  Random prng(Clock::GetRealTimeClock()->TimeInMicroseconds());

  const std::string temp_filename = GetOutputFilePath();

  std::stringstream output_stringstream;
  for (size_t i = 0; i < 2 * file_size_limit; i++) {
    output_stringstream << std::to_string(prng.Rand(0, 9));
  }
  std::string output_str = output_stringstream.str();

  auto output_file = rtc::MakeUnique<RtcEventLogOutputFile>(temp_filename,
                                                            file_size_limit);
  ASSERT_TRUE(output_file->Write(output_str.substr(0, file_size_limit)));
  ASSERT_FALSE(output_file->Write(output_str.substr(file_size_limit,
                                                    2 * file_size_limit)));
  output_file.reset();  // Closing the file flushes the buffer to disk.

  std::ifstream file(temp_filename, std::ios_base::in | std::ios_base::binary);
  ASSERT_TRUE(file.good());
  ASSERT_TRUE(file.is_open());
  std::string file_str((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());

  EXPECT_EQ(output_str.substr(0, file_size_limit), file_str);
}

}  // namespace webrtc
