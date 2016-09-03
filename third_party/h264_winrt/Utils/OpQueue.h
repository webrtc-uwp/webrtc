/*
*  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
*
*  Use of this source code is governed by a BSD-style license
*  that can be found in the LICENSE file in the root of the source
*  tree. An additional intellectual property rights grant can be found
*  in the file PATENTS.  All contributing project authors may
*  be found in the AUTHORS file in the root of the source tree.
*/

/*
This header file defines an object to help queue and serialize
asynchronous operations.

Background:

To perform an operation asynchronously in Media Foundation, an object
does one of the following:

1. Calls MFPutWorkItem(Ex), using either a standard work queue
identifier or a caller-allocated work queue. The work-queue
thread invokes the object's callback.

2. Creates an async result object (IMFAsyncResult) and calls
MFInvokeCallback to invoke the object's callback.

Ultimately, either of these cause the object's callback to be invoked
from a work-queue thread. The object can then complete the operation
inside the callback.

However, the Media Foundation platform may dispatch async callbacks in
parallel on several threads. Putting an item on a work queue does NOT
guarantee that one operation will complete before the next one starts,
or even that work items will be dispatched in the same order they were
called.

To serialize async operations that should not overlap, an object should
use a queue. While one operation is pending, subsequent operations are
put on the queue, and only dispatched after the previous operation is
complete.

The granularity of a single "operation" depends on the requirements of
that particular object. A single operation might involve several
asynchronous calls before the object dispatches the next operation on
the queue.
*/

#ifndef THIRD_PARTY_H264_WINRT_UTILS_OPQUEUE_H_
#define THIRD_PARTY_H264_WINRT_UTILS_OPQUEUE_H_


//-------------------------------------------------------------------
// OpQueue class template
//
// Base class for an async operation queue.
//
// TOperation: The class used to describe operations. This class must
//          implement IUnknown.
//
// The OpQueue class is an abstract class. The derived class must
// implement the following pure-virtual methods:
//
// - IUnknown methods (AddRef, Release, QI)
//
// - DispatchOperation:
//
//      Performs the asynchronous operation specified by pOp.
//
//      At the end of each operation, the derived class must call
//      ProcessQueue to process the next operation in the queue.
//
//      NOTE: An operation is not required to complete inside the
//      DispatchOperation method. A single operation might consist
//      of several asynchronous method calls.
//
// - ValidateOperation:
//
//      Checks whether the object can perform the operation specified
//      by pOp at this time.
//
//      If the object cannot perform the operation now (e.g., because
//      another operation is still in progress) the method should
//      return MF_E_NOTACCEPTING.
//
//-------------------------------------------------------------------

#include <wrl.h>
#include <list>

#include "Async.h"
#include "Utils/CritSec.h"

using Microsoft::WRL::ComPtr;

template <class T, class TOperation>
class OpQueue {
 public:
  typedef std::list<ComPtr<TOperation>> OpList;

  HRESULT QueueOperation(ComPtr<TOperation> pOp);

 protected:
  HRESULT ProcessQueue();
  HRESULT ProcessQueueAsync(IMFAsyncResult *pResult);

  virtual HRESULT DispatchOperation(ComPtr<TOperation> pOp) = 0;
  virtual HRESULT ValidateOperation(ComPtr<TOperation> pOp) = 0;

  OpQueue()
    : m_OnProcessQueue(static_cast<T *>(this),
      &OpQueue::ProcessQueueAsync) {
  }

  virtual ~OpQueue() {
  }

 protected:
  OpList           m_OpQueue;         // Queue of operations.
  CritSec          m_critsec;         // Protects the queue state.
  AsyncCallback<T> m_OnProcessQueue;  // ProcessQueueAsync callback.
};



//-------------------------------------------------------------------
// Place an operation on the queue.
// Public method.
//-------------------------------------------------------------------

template <class T, class TOperation>
HRESULT OpQueue<T, TOperation>::QueueOperation(ComPtr<TOperation> pOp) {
  HRESULT hr = S_OK;

  AutoLock lock(m_critsec);

  m_OpQueue.push_back(pOp);
  hr = ProcessQueue();

  return hr;
}


//-------------------------------------------------------------------
// Process the next operation on the queue.
// Protected method.
//
// Note: This method dispatches the operation to a work queue.
//-------------------------------------------------------------------

template <class T, class TOperation>
HRESULT OpQueue<T, TOperation>::ProcessQueue() {
  HRESULT hr = S_OK;
  if (m_OpQueue.GetCount() > 0) {
    hr = MFPutWorkItem2(
      MFASYNC_CALLBACK_QUEUE_STANDARD,    // Use the standard work queue.
      0,                                  // Default priority
      &m_OnProcessQueue,                  // Callback method.
      nullptr);                           // State object.
  }
  return hr;
}


//-------------------------------------------------------------------
// Process the next operation on the queue.
// Protected method.
//
// Note: This method is called from a work-queue thread.
//-------------------------------------------------------------------

template <class T, class TOperation>
HRESULT OpQueue<T, TOperation>::ProcessQueueAsync(IMFAsyncResult *pResult) {
  HRESULT hr = S_OK;
  ComPtr<TOperation> pOp;

  AutoLock lock(m_critsec);

  if (m_OpQueue.GetCount() > 0) {
    pOp = m_OpQueue.front();

    hr = ValidateOperation(pOp);
    if (SUCCEEDED(hr)) {
      hr = m_OpQueue.RemoveFront(nullptr);
    }
    if (SUCCEEDED(hr)) {
      (void)DispatchOperation(pOp);
    }
  }

  return hr;
}

#endif  // THIRD_PARTY_H264_WINRT_UTILS_OPQUEUE_H_
