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


}  // namespace

TaskQueue::TaskQueue(const char* queue_name)
    : thread_(&TaskQueue::ThreadMain, this, queue_name) {
  RTC_DCHECK(queue_name);
  thread_.Start();
}

TaskQueue::~TaskQueue() {
  RTC_DCHECK(!IsCurrent());

  threadShouldQuit_ = true;
  
  while (!threadDidQuit_) {
    RTC_CHECK_EQ(static_cast<DWORD>(ERROR_NOT_ENOUGH_QUOTA), ::GetLastError());
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
    OrderId order = ++threadPostingOrder_;

    pendingQueue_.push(OrderedQueueTaskPair(order, std::move(task)));
  }

  notifyWake();
}

void TaskQueue::PostDelayedTask(std::unique_ptr<QueuedTask> task,
                                uint32_t milliseconds) {
  
  Time fireAt = now() + Milliseconds(milliseconds);

  DelayedEntryTimeout delay;
  delay.nextFireAt_ = fireAt;

  {
    CritScope lock(&pending_lock_);
    delay.order_ = ++threadPostingOrder_;
    delayedQueue_[delay] = std::move(task);
  }

  notifyWake();
}

void TaskQueue::PostTaskAndReply(std::unique_ptr<QueuedTask> task,
                                 std::unique_ptr<QueuedTask> reply,
                                 TaskQueue* reply_queue) {
  QueuedTask* task_ptr = task.release();
  QueuedTask* reply_task_ptr = reply.release();
  //DWORD reply_thread_id = reply_queue->thread_.GetThreadRef();
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
bool TaskQueue::ThreadMain(void* context) {

  TaskQueue *me = static_cast<TaskQueue *>(context);
  SetQueuePtr(me);

  do
  {
    Microseconds sleepTime {};
    QueueTasksUniPtr runTask;

    auto tick = now();

    {
      CritScope lock(&(me->pending_lock_));

      if (me->delayedQueue_.size() > 0) {
        auto delayedEntry = me->delayedQueue_.begin();
        auto &delayInfo = (*delayedEntry).first;
        auto &delayRun = (*delayedEntry).second;
        if (tick >= delayInfo.nextFireAt_) {
          if (me->pendingQueue_.size() > 0) {
            auto &entry = me->pendingQueue_.front();
            auto &entryOrder = entry.first;
            auto &entryRun = entry.second;
            if (entryOrder < delayInfo.order_) {
              runTask = std::move(entryRun);
              me->pendingQueue_.pop();
              goto process;
            }
          }

          runTask = std::move(delayRun);
          me->delayedQueue_.erase(delayedEntry);
          goto process;
        }

        sleepTime = std::chrono::duration_cast<Microseconds>(delayInfo.nextFireAt_ - tick);
      }

      if (me->pendingQueue_.size() > 0) {
        auto &entry = me->pendingQueue_.front();
        runTask = std::move(entry.second);
        me->pendingQueue_.pop();
      }

      goto process;
    }

  process:
    {
      if (runTask) {
        // process entry immediately then try again
        auto releasePtr = runTask.release();
        if (releasePtr->Run())
          delete releasePtr;

        // attempt to sleep again
        continue;
      }

      if (me->threadShouldQuit_) break;

      std::unique_lock<std::mutex> flagLock(me->flagLock_);
      if (Microseconds() == sleepTime)
        me->flagNotify_.wait(flagLock);
      else
        me->flagNotify_.wait_for(flagLock, sleepTime);
    }
  } while (!me->threadShouldQuit_);

  me->threadDidQuit_ = true;
  return false;
}

void TaskQueue::notifyWake()
{
  flagNotify_.notify_one();
}

}  // namespace rtc
