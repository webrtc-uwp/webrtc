/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/mapping_device_desktop_capturer_wrapper.h"

#include <set>

#include "modules/desktop_capture/fake_desktop_capturer.h"
#include "rtc_base/ptr_util.h"
#include "test/gtest.h"

#include "rtc_base/logging.h"

namespace webrtc {

class SourceIdRememberer : public FakeDesktopCapturer {
 public:
  SourceIdRememberer() = default;
  ~SourceIdRememberer() override = default;

  bool SelectSource(SourceId id) override {
    id_ = id;
    return true;
  }

  SourceId selected_source() const { return id_; }

 private:
  SourceId id_;
};

class MappingDeviceDesktopCapturerWrapperTest
    : public MappingDeviceDesktopCapturerWrapper,
      public testing::Test {
 public:
  MappingDeviceDesktopCapturerWrapperTest()
      : MappingDeviceDesktopCapturerWrapper(
            rtc::MakeUnique<SourceIdRememberer>())
  {}

  ~MappingDeviceDesktopCapturerWrapperTest() override = default;

 protected:
  bool GetExternalSources(std::vector<DetailSource>* sources) override {
    *sources = external_sources_;
    return true;
  }

  bool GetInternalSources(std::vector<DetailSource>* sources) override {
    *sources = internal_sources_;
    return true;
  }

  SourceIdRememberer* base_capturer() const {
    return static_cast<SourceIdRememberer*>(base_capturer_.get());
  }

  static void NoDuplicateIds(const SourceList& list) {
    std::set<SourceId> ids;
    for (const DesktopCapturer::Source& source : list) {
      ASSERT_TRUE(ids.insert(source.id).second);
    }
  }

  std::vector<DetailSource> external_sources_;
  std::vector<DetailSource> internal_sources_;
};

TEST_F(MappingDeviceDesktopCapturerWrapperTest, CommonScenario) {
  external_sources_.push_back({1, "d1"});
  external_sources_.push_back({2, "d2"});
  external_sources_.push_back({3, "d3"});
  internal_sources_.push_back({4, "d1"});
  internal_sources_.push_back({5, "d3"});
  internal_sources_.push_back({6, "d4"});

  SourceList list;
  ASSERT_TRUE(GetSourceList(&list));
  ASSERT_EQ(list.size(), 3u);
  NoDuplicateIds(list);

  // list should be { 1, 3, 4 }.
  ASSERT_TRUE(SelectSource(1));
  ASSERT_EQ(base_capturer()->selected_source(), 4);

  ASSERT_TRUE(SelectSource(3));
  ASSERT_EQ(base_capturer()->selected_source(), 5);

  ASSERT_TRUE(SelectSource(4));
  ASSERT_EQ(base_capturer()->selected_source(), 6);

  ASSERT_FALSE(SelectSource(2));
}

TEST_F(MappingDeviceDesktopCapturerWrapperTest, DuplicateIds) {
  external_sources_.push_back({1, "d1"});
  external_sources_.push_back({2, "d2"});
  external_sources_.push_back({3, "d3"});
  internal_sources_.push_back({1, "d1"});
  internal_sources_.push_back({2, "d3"});
  internal_sources_.push_back({3, "d4"});

  SourceList list;
  ASSERT_TRUE(GetSourceList(&list));
  ASSERT_EQ(list.size(), 3u);
  NoDuplicateIds(list);

  // list should be { 1, 3, 4 }.
  ASSERT_TRUE(SelectSource(1));
  ASSERT_EQ(base_capturer()->selected_source(), 1);

  ASSERT_TRUE(SelectSource(3));
  ASSERT_EQ(base_capturer()->selected_source(), 2);

  ASSERT_TRUE(SelectSource(4));
  ASSERT_EQ(base_capturer()->selected_source(), 3);

  ASSERT_FALSE(SelectSource(2));
}

TEST_F(MappingDeviceDesktopCapturerWrapperTest, DuplicateNames) {
  external_sources_.push_back({1, "d1"});
  external_sources_.push_back({2, "d2"});
  external_sources_.push_back({3, "d3"});
  internal_sources_.push_back({1, "d1"});
  internal_sources_.push_back({2, "d3"});
  internal_sources_.push_back({3, "d3"});

  SourceList list;
  ASSERT_TRUE(GetSourceList(&list));
  ASSERT_EQ(list.size(), 2u);
  NoDuplicateIds(list);

  // list should be { 1, 3 }.
  ASSERT_TRUE(SelectSource(1));
  ASSERT_EQ(base_capturer()->selected_source(), 1);

  ASSERT_TRUE(SelectSource(3));
  ASSERT_EQ(base_capturer()->selected_source(), 2);

  ASSERT_FALSE(SelectSource(2));
  ASSERT_FALSE(SelectSource(4));
}

}  // namespace webrtc
