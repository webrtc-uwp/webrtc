/*
*  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
*
*  Use of this source code is governed by a BSD-style license
*  that can be found in the LICENSE file in the root of the source
*  tree. An additional intellectual property rights grant can be found
*  in the file PATENTS.  All contributing project authors may
*  be found in the AUTHORS file in the root of the source tree.
*/

#ifndef WEBRTC_MODULES_VIDEO_RENDER_WINDOWS_VIDEO_RENDER_WINRT_H_
#define WEBRTC_MODULES_VIDEO_RENDER_WINDOWS_VIDEO_RENDER_WINRT_H_

#include <Map>

#include <windows.ui.xaml.controls.h>

#include <wrl/implements.h>

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/video_render/windows/i_video_render_win.h"
#include "webrtc/modules/video_render/video_render_defines.h"
#include "webrtc/modules/video_render/windows/video_render_source_winrt.h"

namespace webrtc {
class CriticalSectionWrapper;
class EventTimerWrapper;
class Trace;
class ThreadWrapper;


class VideoChannelWinRT : public VideoRenderCallback {
 public:
  VideoChannelWinRT(CriticalSectionWrapper* crit_sect,
                    Trace* trace);

  virtual ~VideoChannelWinRT();

  // Called when the incomming frame size and/or number of
  // streams in mix changes
  virtual int FrameSizeChange(int width, int height, int number_of_streams);

  // A new frame is delivered.
  virtual int DeliverFrame(const VideoFrame& video_frame);
  virtual int32_t RenderFrame(const uint32_t stream_id,
                              const VideoFrame& video_frame);

  // Called to check if the video frame is updated.
  int IsUpdated(bool& is_updated);
  // Called after the video frame has been render to the screen
  int RenderOffFrame();

  Microsoft::WRL::ComPtr<VideoRenderMediaSourceWinRT> GetMediaSource();
  void Lock();
  void Unlock();
  webrtc::VideoFrame& GetVideoFrame();
  int GetWidth();
  int GetHeight();

  void SetStreamSettings(uint16_t stream_id,
                         uint32_t z_order,
                         float start_width,
                         float start_height,
                         float stop_width,
                         float stop_height);
  int GetStreamSettings(uint16_t stream_id,
                        uint32_t& z_order,
                        float& start_width,
                        float& start_height,
                        float& stop_width,
                        float& stop_height);

 private:
  // critical section passed from the owner
  CriticalSectionWrapper* crit_sect_;

  Microsoft::WRL::ComPtr<VideoRenderMediaSourceWinRT> render_media_source_;

  webrtc::VideoFrame video_frame_;

  bool buffer_is_updated_;
  // the frame size
  int width_;
  int height_;
  // stream settings
  uint16_t stream_id_;
  uint32_t z_order_;
  float start_width_;
  float start_height_;
  float stop_width_;
  float stop_height_;
};

class VideoRenderWinRT : IVideoRenderWin {
 public:
  VideoRenderWinRT(Trace* trace, void* h_wnd, bool full_screen);
  ~VideoRenderWinRT();

 public:
  // IVideoRenderWin

  /**************************************************************************
  *
  *   Init
  *
  ***************************************************************************/
  virtual int32_t Init();

  /**************************************************************************
  *
  *   Incoming Streams
  *
  ***************************************************************************/
  virtual VideoRenderCallback* CreateChannel(const uint32_t stream_id,
                                             const uint32_t z_order,
                                             const float left,
                                             const float top,
                                             const float right,
                                             const float bottom);

  virtual int32_t DeleteChannel(const uint32_t stream_id);

  virtual int32_t GetStreamSettings(const uint32_t channel,
                                    const uint16_t stream_id,
                                    uint32_t& z_order,
                                    float& left,
                                    float& top,
                                    float& right,
                                    float& bottom);

  /**************************************************************************
  *
  *   Start/Stop
  *
  ***************************************************************************/

  virtual int32_t StartRender();
  virtual int32_t StopRender();
  /**************************************************************************
  *
  *   Properties
  *
  ***************************************************************************/

  virtual bool IsFullScreen();

  virtual int32_t SetCropping(const uint32_t channel,
                              const uint16_t stream_id,
                              const float left,
                              const float top,
                              const float right,
                              const float bottom);

  virtual int32_t ConfigureRenderer(const uint32_t channel,
                                    const uint16_t stream_id,
                                    const unsigned int z_order,
                                    const float left,
                                    const float top,
                                    const float right,
                                    const float bottom);

  virtual int32_t SetTransparentBackground(const bool enable);

  virtual int32_t ChangeWindow(void* window);

  virtual int32_t GetGraphicsMemory(uint64_t& total_memory,
                                    uint64_t& available_memory);

  virtual int32_t SetText(const uint8_t text_id,
                          const uint8_t* text,
                          const int32_t text_length,
                          const uint32_t color_text,
                          const uint32_t color_bg,
                          const float left,
                          const float top,
                          const float rigth,
                          const float bottom);

  virtual int32_t SetBitmap(const void* bit_map,
                            const uint8_t picture_id,
                            const void* color_key,
                            const float left,
                            const float top,
                            const float right,
                            const float bottom);

 public:
  int UpdateRenderSurface();

 protected:
  // The thread rendering the screen
  static bool ScreenUpdateThreadProc(void* obj);
  bool ScreenUpdateProcess();

 private:
  CriticalSectionWrapper& ref_critsect_;
  Trace* trace_;
  rtc::scoped_ptr<rtc::PlatformThread> screen_update_thread_;
  EventTimerWrapper* screen_update_event_;

  VideoChannelWinRT* channel_;

  bool full_screen_;
  int win_width_;
  int win_height_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_RENDER_WINDOWS_VIDEO_RENDER_WINRT_H_
