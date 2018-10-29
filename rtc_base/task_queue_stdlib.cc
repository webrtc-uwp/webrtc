/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/task_queue.h"

#include <string.h>
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <map>
#include <queue>
#include <utility>

#include "rtc_base/checks.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/event.h"
#include "rtc_base/logging.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/refcount.h"
#include "rtc_base/refcountedobject.h"
#include "rtc_base/thread_annotations.h"

namespace rtc {
namespace {

struct ThreadStartupData {
  Event* started;
  void* thread_context;
};

using Priority = TaskQueue::Priority;
static thread_local void* g_thread_context{};

static void* GetQueuePtrTls() {
  return g_thread_context;
}

static void SetQueuePtr(void* context) {
  g_thread_context = context;
}

static void CALLBACK InitializeQueueThread(ULONG_PTR param) {
  ThreadStartupData* data = reinterpret_cast<ThreadStartupData*>(param);
  SetQueuePtr(data->thread_context);
  data->started->Set();
}

static ThreadPriority TaskQueuePriorityToThreadPriority(Priority priority) {
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

class TaskQueue::Impl : public RefCountInterface {
 public:
  Impl(const char* queue_name, TaskQueue* queue, Priority priority);
  ~Impl() override;

  static TaskQueue::Impl* Current();
  static TaskQueue* CurrentQueue();

  // Used for DCHECKing the current queue.
  bool IsCurrent() const;

  template <class Closure,
            typename std::enable_if<!std::is_convertible<
                Closure,
                std::unique_ptr<QueuedTask>>::value>::type* = nullptr>
  void PostTask(Closure&& closure) {
    PostTask(NewClosure(std::forward<Closure>(closure)));
  }

  void PostTask(std::unique_ptr<QueuedTask> task);
  void PostTaskAndReply(std::unique_ptr<QueuedTask> task,
                        std::unique_ptr<QueuedTask> reply,
                        TaskQueue::Impl* reply_queue);

  void PostDelayedTask(std::unique_ptr<QueuedTask> task, uint32_t milliseconds);

  typedef std::unique_ptr<QueuedTask> QueueTasksUniPtr;
  typedef uint64_t OrderId;
  typedef std::pair<OrderId, QueueTasksUniPtr> OrderedQueueTaskPair;
  typedef std::queue<OrderedQueueTaskPair> QueueTaskQueue;
  typedef std::chrono::milliseconds Milliseconds;
  typedef std::chrono::microseconds Microseconds;
  typedef std::chrono::system_clock::time_point Time;
  typedef std::mutex Lock;

  class WorkerThread : public PlatformThread {
   public:
    WorkerThread(ThreadRunFunction func,
                 void* obj,
                 const char* thread_name,
                 ThreadPriority priority)
        : PlatformThread(func, obj, thread_name, priority) {}

    bool QueueAPC(PAPCFUNC apc_function, ULONG_PTR data) {
      return PlatformThread::QueueAPC(apc_function, data);
    }
  };

  struct DelayedEntryTimeout {
    Time next_fire_at_{};
    OrderId order_{};

    bool operator<(const DelayedEntryTimeout& o) const {
      return std::tie(next_fire_at_, order_) <
             std::tie(o.next_fire_at_, o.order_);
    }
  };

  typedef std::map<DelayedEntryTimeout, QueueTasksUniPtr> DelayTimeoutQueueMap;

  static void ThreadMain(void* context);

  void notifyWake();

  TaskQueue* const queue_;

  std::mutex flag_lock_;
  std::condition_variable flag_notify_;

  WorkerThread thread_;
  std::atomic<bool> thread_should_quit_{};
  std::atomic<bool> thread_did_quit_{};

  rtc::CriticalSection pending_lock_;

  OrderId thread_posting_order_ RTC_GUARDED_BY(pending_lock_){};
  QueueTaskQueue pending_queue_ RTC_GUARDED_BY(pending_lock_);
  DelayTimeoutQueueMap delayed_queue_ RTC_GUARDED_BY(pending_lock_);
};

TaskQueue::Impl::Impl(const char* queue_name,
                      TaskQueue* queue,
                      Priority priority)
    : queue_(queue),
      thread_(&TaskQueue::Impl::ThreadMain,
              this,
              queue_name,
              TaskQueuePriorityToThreadPriority(priority)) {
  RTC_DCHECK(queue_name);
  thread_.Start();
  Event event(false, false);
  ThreadStartupData startup = {&event, this};
  RTC_CHECK(thread_.QueueAPC(&InitializeQueueThread,
                             reinterpret_cast<ULONG_PTR>(&startup)));
  event.Wait(Event::kForever);
}

TaskQueue::Impl::~Impl() {
  RTC_DCHECK(!IsCurrent());

  thread_should_quit_ = true;

  notifyWake();

  while (!thread_did_quit_) {
    // RTC_CHECK_EQ(static_cast<DWORD>(ERROR_NOT_ENOUGH_QUOTA),
    // ::GetLastError());
    Sleep(1);
  }
  thread_.Stop();
}

// static
TaskQueue::Impl* TaskQueue::Impl::Current() {
  return static_cast<TaskQueue::Impl*>(GetQueuePtrTls());
}

// static
TaskQueue* TaskQueue::Impl::CurrentQueue() {
  TaskQueue::Impl* current = Current();
  return current ? current->queue_ : nullptr;
}

bool TaskQueue::Impl::IsCurrent() const {
  return IsThreadRefEqual(thread_.GetThreadRef(), CurrentThreadRef());
}

void TaskQueue::Impl::PostTask(std::unique_ptr<QueuedTask> task) {
  {
    CritScope lock(&pending_lock_);
    OrderId order = ++thread_posting_order_;

    pending_queue_.push(OrderedQueueTaskPair(order, std::move(task)));
  }

  notifyWake();
}

void TaskQueue::Impl::PostDelayedTask(std::unique_ptr<QueuedTask> task,
                                      uint32_t milliseconds) {
  auto fire_at = std::chrono::system_clock::now() + Milliseconds(milliseconds);

  DelayedEntryTimeout delay;
  delay.next_fire_at_ = fire_at;

  {
    CritScope lock(&pending_lock_);
    delay.order_ = ++thread_posting_order_;
    delayed_queue_[delay] = std::move(task);
  }

  notifyWake();
}

void TaskQueue::Impl::PostTaskAndReply(std::unique_ptr<QueuedTask> task,
                                       std::unique_ptr<QueuedTask> reply,
                                       TaskQueue::Impl* reply_queue) {
  QueuedTask* task_ptr = task.release();
  QueuedTask* reply_task_ptr = reply.release();
  PostTask([task_ptr, reply_task_ptr, reply_queue]() {
    if (task_ptr->Run())
      delete task_ptr;

    reply_queue->PostTask(QueueTasksUniPtr(reply_task_ptr));
  });
}

// static
void TaskQueue::Impl::ThreadMain(void* context) {
  TaskQueue::Impl* me = static_cast<TaskQueue::Impl*>(context);

  do {
    Microseconds sleep_time{};
    QueueTasksUniPtr run_task;

    auto tick = std::chrono::system_clock::now();

    {
      CritScope lock(&(me->pending_lock_));

      if (me->delayed_queue_.size() > 0) {
        auto delayed_entry = me->delayed_queue_.begin();
        auto& delay_info = (*delayed_entry).first;
        auto& delay_run = (*delayed_entry).second;
        if (tick >= delay_info.next_fire_at_) {
          if (me->pending_queue_.size() > 0) {
            auto& entry = me->pending_queue_.front();
            auto& entry_order = entry.first;
            auto& entry_run = entry.second;
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

        sleep_time = std::chrono::duration_cast<Microseconds>(
            delay_info.next_fire_at_ - tick);
      }

      if (me->pending_queue_.size() > 0) {
        auto& entry = me->pending_queue_.front();
        run_task = std::move(entry.second);
        me->pending_queue_.pop();
      }

      goto process;
    }

  process : {
    if (run_task) {
      // process entry immediately then try again
      QueuedTask* release_ptr = run_task.release();
      if (release_ptr->Run())
        delete release_ptr;

      // attempt to sleep again
      continue;
    }

    if (me->thread_should_quit_)
      break;

    std::unique_lock<std::mutex> flag_lock(me->flag_lock_);
    if (Microseconds() == sleep_time)
      me->flag_notify_.wait(flag_lock);
    else
      me->flag_notify_.wait_for(flag_lock, sleep_time);
  }
  } while (true);

  me->thread_did_quit_ = true;
}

void TaskQueue::Impl::notifyWake() {
  flag_notify_.notify_one();
}

// Boilerplate for the PIMPL pattern.
TaskQueue::TaskQueue(const char* queue_name, Priority priority)
    : impl_(new RefCountedObject<TaskQueue::Impl>(queue_name, this, priority)) {
}

TaskQueue::~TaskQueue() {}

// static
TaskQueue* TaskQueue::Current() {
  return TaskQueue::Impl::CurrentQueue();
}

// Used for DCHECKing the current queue.
bool TaskQueue::IsCurrent() const {
  return impl_->IsCurrent();
}

void TaskQueue::PostTask(std::unique_ptr<QueuedTask> task) {
  return TaskQueue::impl_->PostTask(std::move(task));
}

void TaskQueue::PostTaskAndReply(std::unique_ptr<QueuedTask> task,
                                 std::unique_ptr<QueuedTask> reply,
                                 TaskQueue* reply_queue) {
  return TaskQueue::impl_->PostTaskAndReply(std::move(task), std::move(reply),
                                            reply_queue->impl_.get());
}

void TaskQueue::PostTaskAndReply(std::unique_ptr<QueuedTask> task,
                                 std::unique_ptr<QueuedTask> reply) {
  return TaskQueue::impl_->PostTaskAndReply(std::move(task), std::move(reply),
                                            impl_.get());
}

void TaskQueue::PostDelayedTask(std::unique_ptr<QueuedTask> task,
                                uint32_t milliseconds) {
  return TaskQueue::impl_->PostDelayedTask(std::move(task), milliseconds);
}

}  // namespace rtc
