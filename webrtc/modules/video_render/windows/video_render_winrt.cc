/*
*  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
*
*  Use of this source code is governed by a BSD-style license
*  that can be found in the LICENSE file in the root of the source
*  tree. An additional intellectual property rights grant can be found
*  in the file PATENTS.  All contributing project authors may
*  be found in the AUTHORS file in the root of the source tree.
*/

#include "webrtc/modules/video_render/windows/video_render_winrt.h"

// System include files
#include <windows.h>

#include <windows.media.core.h>

#include <ppltasks.h>

// WebRtc include files
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"
#include "webrtc/system_wrappers/include/event_wrapper.h"
#include "webrtc/system_wrappers/include/logging.h"
#include "webrtc/base/platform_thread.h"

using Microsoft::WRL::ComPtr;

extern Windows::UI::Core::CoreDispatcher^ g_windowDispatcher;

namespace webrtc {

/*
 *
 *    VideoChannelWinRT
 *
 */
VideoChannelWinRT::VideoChannelWinRT(CriticalSectionWrapper* crit_sect,
                                     Trace* trace)
  : width_(0),
    height_(0),
    buffer_is_updated_(false),
    crit_sect_(crit_sect),
    stream_id_(0),
    z_order_(0),
    start_width_(0),
    start_height_(0),
    stop_width_(0),
    stop_height_(0) {
  VideoRenderMediaSourceWinRT::CreateInstance(&render_media_source_);

  ComPtr<IInspectable> inspectable;
  render_media_source_.As(&inspectable);
}

VideoChannelWinRT::~VideoChannelWinRT() {
  render_media_source_->Stop();
}

void VideoChannelWinRT::SetStreamSettings(uint16_t stream_id,
                                          uint32_t z_order,
                                          float start_width,
                                          float start_height,
                                          float stop_width,
                                          float stop_height) {
  stream_id_ = stream_id;
  z_order_ = z_order;
  start_width_ = start_width;
  start_height_ = start_height;
  stop_width_ = stop_width;
  stop_height_ = stop_height;
}

int VideoChannelWinRT::GetStreamSettings(uint16_t stream_id,
                                         uint32_t& z_order,
                                         float& start_width,
                                         float& start_height,
                                         float& stop_width,
                                         float& stop_height) {
  z_order = z_order_;
  start_width = start_width_;
  start_height = start_height_;
  stop_width = stop_width_;
  stop_height = stop_height_;
  return 0;
}

Microsoft::WRL::ComPtr<VideoRenderMediaSourceWinRT>
VideoChannelWinRT::GetMediaSource() {
  return render_media_source_;
}

void VideoChannelWinRT::Lock() {
  crit_sect_->Enter();
}

void VideoChannelWinRT::Unlock() {
  crit_sect_->Leave();
}

webrtc::VideoFrame& VideoChannelWinRT::GetVideoFrame() {
  return video_frame_;
}

int VideoChannelWinRT::GetWidth() {
  return width_;
}

int VideoChannelWinRT::GetHeight() {
  return height_;
}

// Called from video engine when a the frame size changed
int VideoChannelWinRT::FrameSizeChange(int width,
                                       int height,
                                       int number_of_streams) {
  CriticalSectionScoped cs(crit_sect_);

  LOG(LS_INFO) << "FrameSizeChange, width: " << width <<
    ", height: " << height << ", streams: " << number_of_streams;

  width_ = width;
  height_ = height;

  render_media_source_->FrameSizeChange(width, height);

  return 0;
}

int32_t VideoChannelWinRT::RenderFrame(const uint32_t stream_id,
  const VideoFrame& video_frame) {
  if (width_ != video_frame.width() || height_ != video_frame.height()) {
    if (FrameSizeChange(video_frame.width(), video_frame.height(), 1) == -1) {
      return -1;
    }
  }
  return DeliverFrame(video_frame);
}

// Called from video engine when a new frame should be rendered.
int VideoChannelWinRT::DeliverFrame(const VideoFrame& video_frame) {
  CriticalSectionScoped cs(crit_sect_);

  LOG(LS_VERBOSE) << "DeliverFrame to VideoChannelWinRT";

  if (buffer_is_updated_) {
    LOG(LS_VERBOSE) << "Last frame hasn't been rendered yet. Drop this frame.";
    return -1;
  }

  video_frame_.CopyFrame(video_frame);

  buffer_is_updated_ = true;
  return 0;
}

// Called by channel owner to indicate the frame has been rendered off
int VideoChannelWinRT::RenderOffFrame() {
  CriticalSectionScoped cs(crit_sect_);
  buffer_is_updated_ = false;
  return 0;
}

// Called by channel owner to check if the buffer is updated
int VideoChannelWinRT::IsUpdated(bool& is_updated) {
  CriticalSectionScoped cs(crit_sect_);
  is_updated = buffer_is_updated_;
  return 0;
}

/*
 *
 *    VideoRenderWinRT
 *
 */
VideoRenderWinRT::VideoRenderWinRT(Trace* trace,
                                   void* h_wnd,
                                   bool full_screen)
  : ref_critsect_(*CriticalSectionWrapper::CreateCriticalSection()),
    trace_(trace),
    full_screen_(full_screen),
    channel_(NULL),
    win_width_(0),
    win_height_(0) {
  screen_update_thread_.reset(new rtc::PlatformThread(ScreenUpdateThreadProc,
   this, "VideoRenderWinRT"));
  screen_update_event_ = EventTimerWrapper::Create();
}

VideoRenderWinRT::~VideoRenderWinRT() {
  rtc::scoped_ptr<rtc::PlatformThread> tmpPtr(screen_update_thread_.release());
  if (tmpPtr) {
    screen_update_event_->Set();
    screen_update_event_->StopTimer();

    tmpPtr->Stop();
  }
  delete screen_update_event_;

  delete &ref_critsect_;
}

int32_t VideoRenderWinRT::Init() {
  CriticalSectionScoped cs(&ref_critsect_);

  // Start rendering thread...
  if (!screen_update_thread_) {
    LOG(LS_ERROR) << "Thread not created";
    return -1;
  }
  screen_update_thread_->Start();
  screen_update_thread_->SetPriority(rtc::kRealtimePriority);

  // Start the event triggering the render process
  unsigned int monitorFreq = 60;
  screen_update_event_->StartTimer(true, 1000 / monitorFreq);

  return 0;
}

int32_t VideoRenderWinRT::ChangeWindow(void* window) {
  return -1;
}

int VideoRenderWinRT::UpdateRenderSurface() {
  CriticalSectionScoped cs(&ref_critsect_);

  // Check if there are any updated buffers
  bool updated = false;
  if (channel_ == NULL)
    return -1;
  channel_->IsUpdated(updated);
  if (!updated)
    return -1;

  channel_->Lock();
  int frameLength = 0;
  frameLength += channel_->GetVideoFrame().allocated_size(kYPlane);
  frameLength += channel_->GetVideoFrame().allocated_size(kUPlane);
  frameLength += channel_->GetVideoFrame().allocated_size(kVPlane);
  LOG(LS_VERBOSE) <<
    "Video Render - Update render surface - video frame length: " <<
    frameLength << ", render time: " <<
    channel_->GetVideoFrame().render_time_ms();
  channel_->GetMediaSource()->ProcessVideoFrame(channel_->GetVideoFrame());
  channel_->Unlock();

  // Notice channel that this frame as been rendered
  channel_->RenderOffFrame();

  return 0;
}

/*
 *
 *    Rendering process
 *
 */
bool VideoRenderWinRT::ScreenUpdateThreadProc(void* obj) {
  return static_cast<VideoRenderWinRT*> (obj)->ScreenUpdateProcess();
}

bool VideoRenderWinRT::ScreenUpdateProcess() {
  screen_update_event_->Wait(100);

  if (!screen_update_thread_) {
    // stop the thread
    return false;
  }

  UpdateRenderSurface();

  return true;
}

VideoRenderCallback* VideoRenderWinRT::CreateChannel(const uint32_t stream_id,
                                                     const uint32_t z_order,
                                                     const float left,
                                                     const float top,
                                                     const float right,
                                                     const float bottom) {
  CriticalSectionScoped cs(&ref_critsect_);

  VideoChannelWinRT* channel = new VideoChannelWinRT(&ref_critsect_,
                                                     trace_);
  channel->SetStreamSettings(0, z_order, left, top, right, bottom);

  // store channel
  channel_ = channel;

  return channel;
}

int32_t VideoRenderWinRT::DeleteChannel(const uint32_t streamId) {
  CriticalSectionScoped cs(&ref_critsect_);

  delete channel_;
  channel_ = NULL;

  return 0;
}

int32_t VideoRenderWinRT::GetStreamSettings(const uint32_t channel,
                                            const uint16_t stream_id,
                                            uint32_t& z_order,
                                            float& left,
                                            float& top,
                                            float& right,
                                            float& bottom) {
  if (!channel_)
    return -1;

  channel_->GetStreamSettings(stream_id, z_order, left, top, right, bottom);

  return 0;
}

int32_t VideoRenderWinRT::StartRender() {
  LOG_F(LS_WARNING) << "Not supported.";
  return 0;
}

int32_t VideoRenderWinRT::StopRender() {
  LOG_F(LS_WARNING) << "Not supported.";
  return 0;
}

bool VideoRenderWinRT::IsFullScreen() {
  return full_screen_;
}

int32_t VideoRenderWinRT::SetCropping(const uint32_t channel,
                                      const uint16_t stream_id,
                                      const float left,
                                      const float top,
                                      const float right,
                                      const float bottom) {
  LOG_F(LS_WARNING) << "Not supported.";
  return 0;
}

int32_t VideoRenderWinRT::SetTransparentBackground(const bool enable) {
  LOG_F(LS_WARNING) << "Not supported.";
  return 0;
}

int32_t VideoRenderWinRT::SetText(const uint8_t text_id,
                                  const uint8_t* text,
                                  const int32_t text_length,
                                  const uint32_t color_text,
                                  const uint32_t color_bg,
                                  const float left,
                                  const float top,
                                  const float rigth,
                                  const float bottom) {
  LOG_F(LS_WARNING) << "Not supported.";
  return 0;
}

int32_t VideoRenderWinRT::SetBitmap(const void* bit_map,
                                    const uint8_t picture_id,
                                    const void* color_key,
                                    const float left,
                                    const float top,
                                    const float right,
                                    const float bottom) {
  LOG_F(LS_WARNING) << "Not supported.";
  return 0;
}

int32_t VideoRenderWinRT::GetGraphicsMemory(uint64_t& total_memory,
                                            uint64_t& available_memory) {
  LOG_F(LS_WARNING) << "Not supported.";
  return 0;
}

int32_t VideoRenderWinRT::ConfigureRenderer(const uint32_t channel,
                                            const uint16_t stream_id,
                                            const unsigned int z_order,
                                            const float left,
                                            const float top,
                                            const float right,
                                            const float bottom) {
  LOG_F(LS_WARNING) << "Not supported.";
  return 0;
}

}  // namespace webrtc
