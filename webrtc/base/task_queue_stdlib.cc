/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/task_queue.h"

#include <string.h>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"

typedef std::chrono::system_clock::time_point Time;

namespace rtc {
namespace {

using Priority = TaskQueue::Priority;
static thread_local void *g_thread_context {};

static void *GetQueuePtrTls()
{
  return g_thread_context;
}

static void SetQueuePtr(void *context)
{
  g_thread_context = context;
}

static Time now()
{
  return std::chrono::system_clock::now(); 
}

ThreadPriority TaskQueuePriorityToThreadPriority(Priority priority) {
  switch (priority) {
  case Priority::HIGH:
    return kRealtimePriority;
  case Priority::LOW:
    return kLowPriority;
  case Priority::NORMAL:
    return kNormalPriority;
  default:
    RTC_NOTREACHED();
    break;
  }
  return kNormalPriority;
}

}  // namespace

TaskQueue::TaskQueue(const char* queue_name, Priority priority /*= NORMAL*/)
    : thread_(&TaskQueue::ThreadMain, this, queue_name, TaskQueuePriorityToThreadPriority(priority)) {
  RTC_DCHECK(queue_name);
  thread_.Start();
}

TaskQueue::~TaskQueue() {
  RTC_DCHECK(!IsCurrent());

  thread_should_quit_ = true;

  notifyWake();

  while (!thread_did_quit_) {
    //RTC_CHECK_EQ(static_cast<DWORD>(ERROR_NOT_ENOUGH_QUOTA), ::GetLastError());
    Sleep(1);
  }
  thread_.Stop();
}

// static
TaskQueue* TaskQueue::Current() {
  return static_cast<TaskQueue*>(GetQueuePtrTls());
}

// static
bool TaskQueue::IsCurrent(const char* queue_name) {
  TaskQueue* current = Current();
  return current && current->thread_.name().compare(queue_name) == 0;
}

bool TaskQueue::IsCurrent() const {
  return IsThreadRefEqual(thread_.GetThreadRef(), CurrentThreadRef());
}

void TaskQueue::PostTask(std::unique_ptr<QueuedTask> task) {
  
  {
    CritScope lock(&pending_lock_);
    OrderId order = ++thread_posting_order_;

    pending_queue_.push(OrderedQueueTaskPair(order, std::move(task)));
  }

  notifyWake();
}

void TaskQueue::PostDelayedTask(std::unique_ptr<QueuedTask> task,
                                uint32_t milliseconds) {
  
  Time fire_at = now() + Milliseconds(milliseconds);

  DelayedEntryTimeout delay;
  delay.next_fire_at_ = fire_at;

  {
    CritScope lock(&pending_lock_);
    delay.order_ = ++thread_posting_order_;
    delayed_queue_[delay] = std::move(task);
  }

  notifyWake();
}

void TaskQueue::PostTaskAndReply(std::unique_ptr<QueuedTask> task,
                                 std::unique_ptr<QueuedTask> reply,
                                 TaskQueue* reply_queue) {
  QueuedTask* task_ptr = task.release();
  QueuedTask* reply_task_ptr = reply.release();
  PostTask([task_ptr, reply_task_ptr, reply_queue]() {
    if (task_ptr->Run())
      delete task_ptr;

    reply_queue->PostTask(QueueTasksUniPtr(reply_task_ptr));
  });
}

void TaskQueue::PostTaskAndReply(std::unique_ptr<QueuedTask> task,
                                 std::unique_ptr<QueuedTask> reply) {
  return PostTaskAndReply(std::move(task), std::move(reply), Current());
}

// static
void TaskQueue::ThreadMain(void* context) {

  TaskQueue *me = static_cast<TaskQueue *>(context);
  SetQueuePtr(me);

  do
  {
    Microseconds sleep_time {};
    QueueTasksUniPtr run_task;

    auto tick = now();

    {
      CritScope lock(&(me->pending_lock_));

      if (me->delayed_queue_.size() > 0) {
        auto delayed_entry = me->delayed_queue_.begin();
        auto &delay_info = (*delayed_entry).first;
        auto &delay_run = (*delayed_entry).second;
        if (tick >= delay_info.next_fire_at_) {
          if (me->pending_queue_.size() > 0) {
            auto &entry = me->pending_queue_.front();
            auto &entry_order = entry.first;
            auto &entry_run = entry.second;
            if (entry_order < delay_info.order_) {
              run_task = std::move(entry_run);
              me->pending_queue_.pop();
              goto process;
            }
          }

          run_task = std::move(delay_run);
          me->delayed_queue_.erase(delayed_entry);
          goto process;
        }

        sleep_time = std::chrono::duration_cast<Microseconds>(delay_info.next_fire_at_ - tick);
      }

      if (me->pending_queue_.size() > 0) {
        auto &entry = me->pending_queue_.front();
        run_task = std::move(entry.second);
        me->pending_queue_.pop();
      }

      goto process;
    }

  process:
    {
      if (run_task) {
        // process entry immediately then try again
        auto release_ptr = run_task.release();
        if (release_ptr->Run())
          delete release_ptr;

        // attempt to sleep again
        continue;
      }

      if (me->thread_should_quit_) break;

      std::unique_lock<std::mutex> flag_lock(me->flag_lock_);
      if (Microseconds() == sleep_time)
        me->flag_notify_.wait(flag_lock);
      else
        me->flag_notify_.wait_for(flag_lock, sleep_time);
    }
  } while (true);

  me->thread_did_quit_ = true;
}

void TaskQueue::notifyWake()
{
  flag_notify_.notify_one();
}

}  // namespace rtc
