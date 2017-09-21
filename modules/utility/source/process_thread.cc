/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/utility/include/process_thread.h"

#include "modules/include/module.h"
#include "rtc_base/checks.h"
#include "rtc_base/location.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/thread_checker.h"
#include "rtc_base/trace_event.h"

namespace webrtc {

class ProcessThread::ModuleTask : public rtc::QueuedTask {
 public:
  ModuleTask(rtc::TaskQueue* queue,
             Module* module,
             rtc::Location location,
             bool check_time)
      : queue_(queue),
        module_(module),
        location_(location),
        check_time_(check_time) {}
  bool Run() override {
    RTC_DCHECK_RUN_ON(queue_);
    if (Cancelled()) {
      return true;
    }
    if (!check_time_) {
      module_->Process();
    }
    int64_t until_next_ms = module_->TimeUntilNextProcess();
    check_time_ = false;
    if (until_next_ms <= 0) {
      queue_->PostTask(std::unique_ptr<QueuedTask>(this));
    } else {
      queue_->PostDelayedTask(std::unique_ptr<QueuedTask>(this), until_next_ms);
    }
    return false;
  }

  Module* module() const { return module_; }
  const rtc::Location& location() const { return location_; }

  void Cancel() {
    rtc::CritScope lock(&cs_);
    RTC_DCHECK(!cancelled_);
    cancelled_ = true;
  }

 private:
  bool Cancelled() {
    rtc::CritScope lock(&cs_);
    return cancelled_;
  }

  rtc::TaskQueue* const queue_;
  Module* const module_ RTC_PT_GUARDED_BY(queue_);
  const rtc::Location location_;
  bool check_time_ RTC_ACCESS_ON(queue_);

  rtc::CriticalSection cs_;
  bool cancelled_ RTC_GUARDED_BY(cs_) = false;
};

// static
std::unique_ptr<ProcessThread> ProcessThread::Create(const char* thread_name) {
  return rtc::MakeUnique<ProcessThread>(thread_name);
}

ProcessThread::ProcessThread(const char* thread_name)
    : TaskQueue(thread_name), started_(false) {}
ProcessThread::~ProcessThread() {
  RTC_DCHECK(!started_);
}

void ProcessThread::Start() {
  rtc::CritScope lock(&cs_);
  RTC_DCHECK(!started_);
  for (ModuleTask* task : modules_) {
    task->module()->ProcessThreadAttached(this);
    PostTask(std::unique_ptr<rtc::QueuedTask>(task));
  }
  started_ = true;
}

void ProcessThread::Stop() {
  rtc::CritScope lock(&cs_);
  if (!started_)
    return;
  for (ModuleTask* task : modules_) {
    task->module()->ProcessThreadAttached(nullptr);
    task->Cancel();
  }
  modules_.clear();
  started_ = false;
}

void ProcessThread::WakeUp(Module* module) {
  rtc::CritScope lock(&cs_);
  RTC_DCHECK(started_);
  for (ModuleTask*& task : modules_) {
    if (task->module() == module) {
      auto* new_task = new ModuleTask(this, module, task->location(), false);
      task->Cancel();
      task = new_task;
      PostTask(std::unique_ptr<rtc::QueuedTask>(new_task));
      return;
    }
  }
  RTC_NOTREACHED() << "Module to wakeup not found.";
}

void ProcessThread::RegisterModule(Module* module, const rtc::Location& from) {
  auto* new_task = new ModuleTask(this, module, from, true);
  rtc::CritScope lock(&cs_);
  if (started_) {
    module->ProcessThreadAttached(this);
    PostTask(std::unique_ptr<rtc::QueuedTask>(new_task));
  }
  modules_.push_back(new_task);
}

// Removes a previously registered module.
// Can be called from any thread.
void ProcessThread::DeRegisterModule(Module* module) {
  rtc::CritScope lock(&cs_);
  for (auto it = modules_.begin(); it != modules_.end(); ++it) {
    if ((*it)->module() == module) {
      module->ProcessThreadAttached(nullptr);
      (*it)->Cancel();
      modules_.erase(it);
      return;
    }
  }
}

}  // namespace webrtc
