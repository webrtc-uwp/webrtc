/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_UTILITY_INCLUDE_PROCESS_THREAD_H_
#define MODULES_UTILITY_INCLUDE_PROCESS_THREAD_H_

#include <memory>

#include "typedefs.h"  // NOLINT(build/include)

#include "rtc_base/criticalsection.h"
#include "rtc_base/task_queue.h"

namespace rtc {
class Location;
}  // namespace rtc

namespace webrtc {
class Module;

// TODO(tommi): ProcessThread probably doesn't need to be a virtual
// interface.  There exists one override besides ProcessThreadImpl,
// MockProcessThread, but when looking at how it is used, it seems
// a nullptr might suffice (or simply an actual ProcessThread instance).
class ProcessThread : public rtc::TaskQueue {
 public:
  static std::unique_ptr<ProcessThread> Create(const char* thread_name);

  ProcessThread(const char* thread_name);
  ~ProcessThread();

  // Starts the worker thread.  Must be called from the construction thread.
  void Start();

  // Stops the worker thread.  Must be called from the construction thread.
  void Stop();

  // Wakes the thread up to give a module a chance to do processing right
  // away.  This causes the worker thread to wake up and requery the specified
  // module for when it should be called back. (Typically the module should
  // return 0 from TimeUntilNextProcess on the worker thread at that point).
  // Can be called on any thread.
  void WakeUp(Module* module);

  // Adds a module that will start to receive callbacks on the worker thread.
  // Can be called from any thread.
  void RegisterModule(Module* module, const rtc::Location& from);

  // Removes a previously registered module.
  // Can be called from any thread.
  void DeRegisterModule(Module* module);

 private:
  class ModuleTask;
  rtc::CriticalSection cs_;  // Used to guard modules_, tasks_ and stop_.

  bool started_ RTC_GUARDED_BY(cs_);
  std::vector<ModuleTask*> modules_ RTC_GUARDED_BY(cs_);
};

}  // namespace webrtc

#endif // MODULES_UTILITY_INCLUDE_PROCESS_THREAD_H_
