/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/system_wrappers/source/event_timer_win.h"

#ifndef WINUWP
#include "Mmsystem.h"
#else
using namespace Windows::System::Threading;
using namespace Windows::Foundation;
#endif

namespace webrtc {

// static
EventTimerWrapper* EventTimerWrapper::Create() {
  return new EventTimerWin();
}

#ifndef WINUWP
class EventTimerWin::Impl
{
public:
  Impl()
    : event_(::CreateEvent(NULL,    // security attributes
                           FALSE,   // manual reset
                           FALSE,   // initial state
                           NULL)),  // name of event
      timerID_(NULL)
  {
  }

  ~Impl()
  {
    StopTimer();
    CloseHandle(event_);
  }

  bool Set() {
    // Note: setting an event that is already set has no effect.
    return SetEvent(event_) == 1;
  }

  EventTypeWrapper Wait(unsigned long max_time) {
    unsigned long res = WaitForSingleObject(event_, max_time);
    switch (res) {
    case WAIT_OBJECT_0:
      return kEventSignaled;
    case WAIT_TIMEOUT:
      return kEventTimeout;
    default:
      return kEventError;
    }
  }

  bool StartTimer(bool periodic, unsigned long time) {
      if (timerID_ != NULL) {
          timeKillEvent(timerID_);
          timerID_ = NULL;
      }

      if (periodic) {
          timerID_ = timeSetEvent(time, 0, (LPTIMECALLBACK)HANDLE(event_), 0,
              TIME_PERIODIC | TIME_CALLBACK_EVENT_PULSE);
      }
      else {
          timerID_ = timeSetEvent(time, 0, (LPTIMECALLBACK)HANDLE(event_), 0,
              TIME_ONESHOT | TIME_CALLBACK_EVENT_SET);
      }

      return timerID_ != NULL;
  }

  bool StopTimer() {
      if (timerID_ != NULL) {
          timeKillEvent(timerID_);
          timerID_ = NULL;
      }

      return true;
  }


private:
  HANDLE  event_;
  uint32_t timerID_;
};

#else // WinUWP
class EventTimerWin::Impl
{
public:
    Impl()
        : event_(::CreateEvent(NULL,    // security attributes
        FALSE,   // manual reset
        FALSE,   // initial state
        NULL)),  // name of event
        timer_(nullptr)
    {

    }

    ~Impl()
    {
        StopTimer();
        CloseHandle(event_);
    }

    bool Set() {
        // Note: setting an event that is already set has no effect.
        return SetEvent(event_) == 1;
    }

    bool Reset() {
        return ResetEvent(event_) == 1;
    }

    EventTypeWrapper Wait(unsigned long max_time) {
        unsigned long res = WaitForSingleObject(event_, max_time);
        switch (res) {
        case WAIT_OBJECT_0:
            return kEventSignaled;
        case WAIT_TIMEOUT:
            return kEventTimeout;
        default:
            return kEventError;
        }
    }

    bool StartTimer(bool periodic, unsigned long time) {
        if (timer_ != nullptr) {
            timer_->Cancel();
            timer_ = nullptr;
        }

        // Duration specified in 100ns units.
        TimeSpan period = { time * 10000 };

        if (periodic) {
            timer_ = ThreadPoolTimer::CreatePeriodicTimer(
                ref new TimerElapsedHandler([this](ThreadPoolTimer^ source)
            {
                this->Set();
            }), period);
        }
        else {
            timer_ = ThreadPoolTimer::CreateTimer(
                ref new TimerElapsedHandler([this](ThreadPoolTimer^ source)
            {
                this->Set();
            }), period);
        }

        return timer_ != nullptr;
    }

    bool StopTimer() {
        if (timer_ != nullptr) {
            timer_->Cancel();
            timer_ = nullptr;
        }

        return true;
    }


private:
    HANDLE  event_;
    ThreadPoolTimer^ timer_;
};

#endif


EventTimerWin::EventTimerWin()
    : pimpl_(new Impl())
{
}

EventTimerWin::~EventTimerWin() {
}

bool EventTimerWin::Set() {
    return pimpl_->Set();
}

EventTypeWrapper EventTimerWin::Wait(unsigned long max_time) {
    return pimpl_->Wait(max_time);
}

bool EventTimerWin::StartTimer(bool periodic, unsigned long time) {
    return pimpl_->StartTimer(periodic, time);
}

bool EventTimerWin::StopTimer() {
    return pimpl_->StopTimer();
}

}  // namespace webrtc
