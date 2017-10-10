/*
*  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
*
*  Use of this source code is governed by a BSD-style license
*  that can be found in the LICENSE file in the root of the source
*  tree. An additional intellectual property rights grant can be found
*  in the file PATENTS.  All contributing project authors may
*  be found in the AUTHORS file in the root of the source tree.
*/

#include "webrtc/modules/video_capture/windows/video_capture_sink_winuwp.h"

#include <ppltasks.h>

#include <strsafe.h>

#include <mferror.h>
#include <mfapi.h>

#include <windows.foundation.h>

#include "webrtc/system_wrappers/include/critical_section_wrapper.h"
#include "webrtc/system_wrappers/include/logging.h"

using Microsoft::WRL::ComPtr;
using Windows::Foundation::IPropertyValue;
using Windows::Foundation::PropertyType;
using Windows::Media::IMediaExtension;
using Windows::Media::MediaProperties::IMediaEncodingProperties;

namespace {

inline void ThrowIfError(HRESULT hr) {
  if (FAILED(hr)) {
    throw ref new Platform::Exception(hr);
  }
}

inline void Throw(HRESULT hr) {
  assert(FAILED(hr));
  throw ref new Platform::Exception(hr);
}

static void AddAttribute(_In_ GUID guidKey, _In_ IPropertyValue ^value,
  _In_ IMFAttributes *pAttr) {
  PropertyType type = value->Type;
  switch (type) {
  case PropertyType::UInt8Array:
    {
      Platform::Array<BYTE>^ arr;
      value->GetUInt8Array(&arr);

      ThrowIfError(pAttr->SetBlob(guidKey, arr->Data, arr->Length));
    }
    break;

  case PropertyType::Double:
    {
      ThrowIfError(pAttr->SetDouble(guidKey, value->GetDouble()));
    }
    break;

  case PropertyType::Guid:
    {
      ThrowIfError(pAttr->SetGUID(guidKey, value->GetGuid()));
    }
    break;

  case PropertyType::String:
    {
      ThrowIfError(pAttr->SetString(guidKey, value->GetString()->Data()));
    }
    break;

  case PropertyType::UInt32:
    {
      ThrowIfError(pAttr->SetUINT32(guidKey, value->GetUInt32()));
    }
    break;

  case PropertyType::UInt64:
    {
      ThrowIfError(pAttr->SetUINT64(guidKey, value->GetUInt64()));
    }
    break;
  }
}

void ConvertPropertiesToMediaType(
    _In_ IMediaEncodingProperties ^mep,
    _Outptr_ IMFMediaType **ppMT) {
    if (mep == nullptr || ppMT == nullptr) {
      throw ref new Platform::InvalidArgumentException();
    }
    ComPtr<IMFMediaType> spMT;
    *ppMT = nullptr;
    ThrowIfError(MFCreateMediaType(&spMT));

    auto it = mep->Properties->First();

    while (it->HasCurrent) {
      auto currentValue = it->Current;
      AddAttribute(currentValue->Key,
        safe_cast<IPropertyValue^>(currentValue->Value),
        spMT.Get());
      it->MoveNext();
    }

    GUID guiMajorType = safe_cast<IPropertyValue^>(
      mep->Properties->Lookup(MF_MT_MAJOR_TYPE))->GetGuid();

    if (guiMajorType != MFMediaType_Video) {
      Throw(E_UNEXPECTED);
    }

    *ppMT = spMT.Detach();
  }

  DWORD GetStreamId() {
      return 0;
  }
}  // namespace

namespace webrtc {
namespace videocapturemodule {

VideoCaptureStreamSinkWinUWP::VideoCaptureStreamSinkWinUWP(DWORD dwIdentifier)
  : _cRef(1),
    _critSec(CriticalSectionWrapper::CreateCriticalSection()),
    _dwIdentifier(dwIdentifier),
    _state(State_TypeNotSet),
    _isShutdown(false),
    _fGetStartTimeFromSample(false),
    _startTime(0),
    _workQueueId(0),
    _pParent(nullptr),
    _workQueueCB(this, &VideoCaptureStreamSinkWinUWP::OnDispatchWorkItem) {
  ZeroMemory(&_guiCurrentSubtype, sizeof(_guiCurrentSubtype));
}

VideoCaptureStreamSinkWinUWP::~VideoCaptureStreamSinkWinUWP() {
  assert(_isShutdown);
  if (_critSec) {
    delete _critSec;
  }
}

// IUnknown methods
IFACEMETHODIMP VideoCaptureStreamSinkWinUWP::QueryInterface(
    REFIID riid,
    void **ppv) {
  if (ppv == nullptr) {
    return E_POINTER;
  }
  (*ppv) = nullptr;

  HRESULT hr = S_OK;
  if (riid == IID_IUnknown ||
    riid == IID_IMFStreamSink ||
    riid == IID_IMFMediaEventGenerator) {
    (*ppv) = static_cast<IMFStreamSink*>(this);
    AddRef();
  } else if (riid == IID_IMFMediaTypeHandler) {
    (*ppv) = static_cast<IMFMediaTypeHandler*>(this);
    AddRef();
  } else {
    hr = E_NOINTERFACE;
  }

  return hr;
}

IFACEMETHODIMP_(ULONG) VideoCaptureStreamSinkWinUWP::AddRef() {
  return InterlockedIncrement(&_cRef);
}

IFACEMETHODIMP_(ULONG) VideoCaptureStreamSinkWinUWP::Release() {
  int64 cRef = InterlockedDecrement(&_cRef);
  if (cRef == 0) {
    delete this;
  }
  return cRef;
}

// IMFMediaEventGenerator methods.
// Note: These methods call through to the event queue helper object.
IFACEMETHODIMP VideoCaptureStreamSinkWinUWP::BeginGetEvent(
    IMFAsyncCallback *pCallback,
    IUnknown *punkState) {
  HRESULT hr = S_OK;

  CriticalSectionScoped cs(_critSec);

  if (_isShutdown) {
    hr = MF_E_SHUTDOWN;
  }

  if (SUCCEEDED(hr)) {
    hr = _spEventQueue->BeginGetEvent(pCallback, punkState);
  }

  if (!SUCCEEDED(hr)) {
    LOG_F(LS_ERROR) << "Capture stream sink error: " << hr;
  }

  return hr;
}

IFACEMETHODIMP VideoCaptureStreamSinkWinUWP::EndGetEvent(
    IMFAsyncResult *pResult,
    IMFMediaEvent **ppEvent) {
  HRESULT hr = S_OK;

  CriticalSectionScoped cs(_critSec);

  if (_isShutdown) {
    hr = MF_E_SHUTDOWN;
  }

  if (SUCCEEDED(hr)) {
    hr = _spEventQueue->EndGetEvent(pResult, ppEvent);
  }

  if (!SUCCEEDED(hr)) {
    LOG_F(LS_ERROR) << "Capture stream sink error: " << hr;
  }

  return hr;
}

IFACEMETHODIMP VideoCaptureStreamSinkWinUWP::GetEvent(
    DWORD dwFlags,
    IMFMediaEvent **ppEvent) {
  // NOTE:
  // GetEvent can block indefinitely, so we don't hold the lock.
  // This requires some juggling with the event queue pointer.

  HRESULT hr = S_OK;

  ComPtr<IMFMediaEventQueue> spQueue;

  {
    CriticalSectionScoped cs(_critSec);

    // Check shutdown
    if (_isShutdown) {
      hr = MF_E_SHUTDOWN;
    }

    // Get the pointer to the event queue.
    if (SUCCEEDED(hr)) {
      spQueue = _spEventQueue;
    }
  }

  // Now get the event.
  if (SUCCEEDED(hr)) {
    hr = spQueue->GetEvent(dwFlags, ppEvent);
  }

  if (!SUCCEEDED(hr)) {
    LOG_F(LS_ERROR) << "Capture stream sink error: " << hr;
  }

  return hr;
}

IFACEMETHODIMP VideoCaptureStreamSinkWinUWP::QueueEvent(
    MediaEventType met,
    REFGUID guidExtendedType,
    HRESULT hrStatus,
    PROPVARIANT const *pvValue) {
  HRESULT hr = S_OK;

  CriticalSectionScoped cs(_critSec);

  if (_isShutdown) {
    hr = MF_E_SHUTDOWN;
  }

  if (SUCCEEDED(hr)) {
    hr = _spEventQueue->QueueEventParamVar(
      met, guidExtendedType, hrStatus, pvValue);
  }

  if (!SUCCEEDED(hr)) {
    LOG_F(LS_ERROR) << "Capture stream sink error: " << hr;
  }

  return hr;
}

/// IMFStreamSink methods
IFACEMETHODIMP VideoCaptureStreamSinkWinUWP::GetMediaSink(
    IMFMediaSink **ppMediaSink) {
  if (ppMediaSink == nullptr) {
    return E_INVALIDARG;
  }

  CriticalSectionScoped cs(_critSec);

  HRESULT hr = S_OK;
  if (_isShutdown) {
    hr = MF_E_SHUTDOWN;
  }

  if (SUCCEEDED(hr)) {
    _spSink.Get()->QueryInterface(IID_IMFMediaSink,
      reinterpret_cast<void**>(ppMediaSink));
  } else {
    LOG_F(LS_ERROR) << "Capture stream sink error: " << hr;
  }

  return hr;
}

IFACEMETHODIMP VideoCaptureStreamSinkWinUWP::GetIdentifier(
    DWORD *pdwIdentifier) {
  if (pdwIdentifier == nullptr) {
    return E_INVALIDARG;
  }

  CriticalSectionScoped cs(_critSec);

  HRESULT hr = S_OK;
  if (_isShutdown) {
    hr = MF_E_SHUTDOWN;
  }

  if (SUCCEEDED(hr)) {
    *pdwIdentifier = _dwIdentifier;
  } else {
    LOG_F(LS_ERROR) << "Capture stream sink error: " << hr;
  }

  return hr;
}

IFACEMETHODIMP VideoCaptureStreamSinkWinUWP::GetMediaTypeHandler(
    IMFMediaTypeHandler **ppHandler) {
  if (ppHandler == nullptr) {
    return E_INVALIDARG;
  }

  CriticalSectionScoped cs(_critSec);

  HRESULT hr = S_OK;
  if (_isShutdown) {
    hr = MF_E_SHUTDOWN;
  }

  // This stream object acts as its own type handler, so we QI ourselves.
  if (SUCCEEDED(hr)) {
    hr = QueryInterface(IID_IMFMediaTypeHandler,
      reinterpret_cast<void**>(ppHandler));
  }

  if (!SUCCEEDED(hr)) {
    LOG_F(LS_ERROR) << "Capture stream sink error: " << hr;
  }

  return hr;
}

IFACEMETHODIMP VideoCaptureStreamSinkWinUWP::ProcessSample(IMFSample *pSample) {
  if (pSample == nullptr) {
    return E_INVALIDARG;
  }

  HRESULT hr = S_OK;

  CriticalSectionScoped cs(_critSec);

  if (_isShutdown) {
    hr = MF_E_SHUTDOWN;
  }

  if (SUCCEEDED(hr)) {
    hr = ValidateOperation(OpProcessSample);
  }

  if (SUCCEEDED(hr)) {
    if (SUCCEEDED(hr)) {
      _sampleQueue.push(pSample);
    }

    // Unless we are paused, start an async operation to
    // dispatch the next sample.
    if (SUCCEEDED(hr)) {
      if (_state != State_Paused) {
        hr = QueueAsyncOperation(OpProcessSample);
      }
    }
  }

  if (!SUCCEEDED(hr)) {
    LOG_F(LS_ERROR) << "Capture stream sink error: " << hr;
  }

  return hr;
}

IFACEMETHODIMP VideoCaptureStreamSinkWinUWP::PlaceMarker(
    MFSTREAMSINK_MARKER_TYPE eMarkerType,
    const PROPVARIANT *pvarMarkerValue,
    const PROPVARIANT *pvarContextValue) {

  CriticalSectionScoped cs(_critSec);

  HRESULT hr = S_OK;
  ComPtr<IMarker> spMarker;

  if (_isShutdown) {
    hr = MF_E_SHUTDOWN;
  }

  if (SUCCEEDED(hr)) {
    hr = ValidateOperation(OpPlaceMarker);
  }

  if (SUCCEEDED(hr)) {
    hr = Marker::Create(eMarkerType, pvarMarkerValue, pvarContextValue, &spMarker);
  }

  if (SUCCEEDED(hr)) {
    _sampleQueue.push(spMarker.Get());
  }

  // Unless we are paused, start an async operation to
  // dispatch the next sample/marker.
  if (SUCCEEDED(hr)) {
    if (_state != State_Paused) {
      hr = QueueAsyncOperation(OpPlaceMarker);
    }
  }

  if (!SUCCEEDED(hr)) {
    LOG_F(LS_ERROR) << "Capture stream sink error: " << hr;
  }

  return hr;
}

// Discards all samples that were not processed yet.
IFACEMETHODIMP VideoCaptureStreamSinkWinUWP::Flush() {
  CriticalSectionScoped cs(_critSec);
  HRESULT hr = S_OK;
  try {
    if (_isShutdown) {
      hr = MF_E_SHUTDOWN;
    }
    ThrowIfError(hr);

    DropSamplesFromQueue();
  } catch (Platform::Exception ^exc) {
    hr = exc->HResult;
  }

  if (!SUCCEEDED(hr)) {
    LOG_F(LS_ERROR) << "Capture stream sink error: " << hr;
  }

  return hr;
}

/// IMFMediaTypeHandler methods
// Check if a media type is supported.
IFACEMETHODIMP VideoCaptureStreamSinkWinUWP::IsMediaTypeSupported(
  /* [in] */ IMFMediaType *pMediaType,
  /* [out] */ IMFMediaType **ppMediaType) {
  if (pMediaType == nullptr) {
    return E_INVALIDARG;
  }

  CriticalSectionScoped cs(_critSec);

  GUID majorType = GUID_NULL;

  HRESULT hr = S_OK;
  if (_isShutdown) {
    hr = MF_E_SHUTDOWN;
  }

  if (SUCCEEDED(hr)) {
    hr = pMediaType->GetGUID(MF_MT_MAJOR_TYPE, &majorType);
  }

  // First make sure it's video or audio type.
  if (SUCCEEDED(hr)) {
    if (majorType != MFMediaType_Video) {
      hr = MF_E_INVALIDTYPE;
    }
  }

  if (SUCCEEDED(hr) && _spCurrentType != nullptr) {
    GUID guiNewSubtype;
    if (FAILED(pMediaType->GetGUID(MF_MT_SUBTYPE, &guiNewSubtype)) ||
      guiNewSubtype != _guiCurrentSubtype) {
      hr = MF_E_INVALIDTYPE;
    }
  }

  if (ppMediaType) {
    *ppMediaType = nullptr;
  }

  if (!SUCCEEDED(hr)) {
    LOG_F(LS_ERROR) << "Capture stream sink error: " << hr;
  }

  return hr;
}

// Return the number of preferred media types.
IFACEMETHODIMP VideoCaptureStreamSinkWinUWP::GetMediaTypeCount(
    DWORD *pdwTypeCount) {
  if (pdwTypeCount == nullptr) {
    return E_INVALIDARG;
  }

  CriticalSectionScoped cs(_critSec);

  HRESULT hr = S_OK;
  if (_isShutdown) {
    hr = MF_E_SHUTDOWN;
  }

  if (SUCCEEDED(hr)) {
    *pdwTypeCount = 1;
  } else {
    LOG_F(LS_ERROR) << "Capture stream sink error: " << hr;
  }

  return hr;
}


// Return a preferred media type by index.
IFACEMETHODIMP VideoCaptureStreamSinkWinUWP::GetMediaTypeByIndex(
  /* [in] */ DWORD dwIndex,
  /* [out] */ IMFMediaType **ppType) {
  if (ppType == nullptr) {
    return E_INVALIDARG;
  }

  CriticalSectionScoped cs(_critSec);

  HRESULT hr = S_OK;
  if (_isShutdown) {
    hr = MF_E_SHUTDOWN;
  }

  if (dwIndex > 0) {
    hr = MF_E_NO_MORE_TYPES;
  } else {
    *ppType = _spCurrentType.Get();
    if (*ppType != nullptr) {
      (*ppType)->AddRef();
    }
  }

  if (!SUCCEEDED(hr)) {
    LOG_F(LS_ERROR) << "Capture stream sink error: " << hr;
  }

  return hr;
}


// Set the current media type.
IFACEMETHODIMP VideoCaptureStreamSinkWinUWP::SetCurrentMediaType(
    IMFMediaType *pMediaType) {
  HRESULT hr = S_OK;
  try {
    if (pMediaType == nullptr) {
      Throw(E_INVALIDARG);
    }
    CriticalSectionScoped cs(_critSec);

    if (_isShutdown) {
      hr = MF_E_SHUTDOWN;
    }
    ThrowIfError(hr);

    // We don't allow format changes after streaming starts.
    ThrowIfError(ValidateOperation(OpSetMediaType));

    // We set media type already
    if (_state >= State_Ready) {
      ThrowIfError(IsMediaTypeSupported(pMediaType, nullptr));
    }

    GUID guiMajorType;
    pMediaType->GetMajorType(&guiMajorType);

    ThrowIfError(MFCreateMediaType(_spCurrentType.ReleaseAndGetAddressOf()));
    ThrowIfError(pMediaType->CopyAllItems(_spCurrentType.Get()));
    ThrowIfError(_spCurrentType->GetGUID(MF_MT_SUBTYPE, &_guiCurrentSubtype));
    if (_state < State_Ready) {
      _state = State_Ready;
    } else if (_state > State_Ready) {
      ComPtr<IMFMediaType> spType;
      ThrowIfError(MFCreateMediaType(&spType));
      ThrowIfError(pMediaType->CopyAllItems(spType.Get()));
      ProcessFormatChange(spType.Get());
    }
  } catch (Platform::Exception ^exc) {
    hr = exc->HResult;
  }

  if (!SUCCEEDED(hr)) {
    LOG_F(LS_ERROR) << "Capture stream sink error: " << hr;
  }

  return hr;
}

// Return the current media type, if any.
IFACEMETHODIMP VideoCaptureStreamSinkWinUWP::GetCurrentMediaType(
    IMFMediaType **ppMediaType) {
  if (ppMediaType == nullptr) {
    return E_INVALIDARG;
  }

  CriticalSectionScoped cs(_critSec);

  HRESULT hr = S_OK;
  if (_isShutdown) {
    hr = MF_E_SHUTDOWN;
  }

  if (SUCCEEDED(hr)) {
    if (_spCurrentType == nullptr) {
      hr = MF_E_NOT_INITIALIZED;
    }
  }

  if (SUCCEEDED(hr)) {
    *ppMediaType = _spCurrentType.Get();
    (*ppMediaType)->AddRef();
  } else {
    LOG_F(LS_ERROR) << "Capture stream sink error: " << hr;
  }

  return hr;
}


// Return the major type GUID.
IFACEMETHODIMP VideoCaptureStreamSinkWinUWP::GetMajorType(GUID *pguidMajorType) {
  if (pguidMajorType == nullptr) {
    return E_INVALIDARG;
  }

  if (!_spCurrentType) {
    return MF_E_NOT_INITIALIZED;
  }

  *pguidMajorType = MFMediaType_Video;

  return S_OK;
}


// private methods
HRESULT VideoCaptureStreamSinkWinUWP::Initialize(
    VideoCaptureMediaSinkWinUWP *pParent,
    ISinkCallback ^callback) {
  assert(pParent != nullptr);

  HRESULT hr = S_OK;

  // Create the event queue helper.
  hr = MFCreateEventQueue(&_spEventQueue);

  // Allocate a new work queue for async operations.
  if (SUCCEEDED(hr)) {
    hr = MFAllocateSerialWorkQueue(
        MFASYNC_CALLBACK_QUEUE_STANDARD, &_workQueueId);
  }

  if (SUCCEEDED(hr)) {
    _spSink = pParent;
    _pParent = pParent;
    _callback = callback;
  } else {
    LOG_F(LS_ERROR) << "Capture stream sink error: " << hr;
  }

  return hr;
}


// Called when the presentation clock starts.
HRESULT VideoCaptureStreamSinkWinUWP::Start(MFTIME start) {
  CriticalSectionScoped cs(_critSec);

  HRESULT hr = S_OK;

  hr = ValidateOperation(OpStart);

  if (SUCCEEDED(hr)) {
    if (start != PRESENTATION_CURRENT_POSITION) {
      _startTime = start;        // Cache the start time.
      _fGetStartTimeFromSample = false;
    } else {
      _fGetStartTimeFromSample = true;
    }
    _state = State_Started;
    hr = QueueAsyncOperation(OpStart);
  }

  if (!SUCCEEDED(hr)) {
    LOG_F(LS_ERROR) << "Capture stream sink error: " << hr;
  }

  return hr;
}

// Called when the presentation clock stops.
HRESULT VideoCaptureStreamSinkWinUWP::Stop() {
  CriticalSectionScoped cs(_critSec);

  HRESULT hr = S_OK;

  hr = ValidateOperation(OpStop);

  if (SUCCEEDED(hr)) {
    _state = State_Stopped;
    hr = QueueAsyncOperation(OpStop);
  }

  if (!SUCCEEDED(hr)) {
    LOG_F(LS_ERROR) << "Capture stream sink error: " << hr;
  }

  return hr;
}

// Called when the presentation clock pauses.
HRESULT VideoCaptureStreamSinkWinUWP::Pause() {
  CriticalSectionScoped cs(_critSec);

  HRESULT hr = S_OK;

  hr = ValidateOperation(OpPause);

  if (SUCCEEDED(hr)) {
    _state = State_Paused;
    hr = QueueAsyncOperation(OpPause);
  }

  if (!SUCCEEDED(hr)) {
    LOG_F(LS_ERROR) << "Capture stream sink error: " << hr;
  }

  return hr;
}

// Called when the presentation clock restarts.
HRESULT VideoCaptureStreamSinkWinUWP::Restart() {
  CriticalSectionScoped cs(_critSec);

  HRESULT hr = S_OK;

  hr = ValidateOperation(OpRestart);

  if (SUCCEEDED(hr)) {
    _state = State_Started;
    hr = QueueAsyncOperation(OpRestart);
  }

  if (!SUCCEEDED(hr)) {
    LOG_F(LS_ERROR) << "Capture stream sink error: " << hr;
  }

  return hr;
}

// Class-static matrix of operations vs states.
// If an entry is TRUE, the operation is valid from that state.
BOOL VideoCaptureStreamSinkWinUWP::ValidStateMatrix
    [VideoCaptureStreamSinkWinUWP::State_Count]
    [VideoCaptureStreamSinkWinUWP::Op_Count] = {
  // States:    Operations:
  //            SetType  Start  Restart  Pause  Stop  Sample Marker
  /* NotSet */  TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,

  /* Ready */   TRUE, TRUE, FALSE, TRUE, TRUE, FALSE, TRUE,

  /* Start */   TRUE, TRUE, FALSE, TRUE, TRUE, TRUE, TRUE,

  /* Pause */   TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE,

  /* Stop */    TRUE, TRUE, FALSE, FALSE, TRUE, FALSE, TRUE,
};

// Checks if an operation is valid in the current state.
HRESULT VideoCaptureStreamSinkWinUWP::ValidateOperation(StreamOperation op) {
  assert(!_isShutdown);

  if (ValidStateMatrix[_state][op]) {
    return S_OK;
  } else if (_state == State_TypeNotSet) {
    return MF_E_NOT_INITIALIZED;
  } else {
    return MF_E_INVALIDREQUEST;
  }
}

// Shuts down the stream sink.
HRESULT VideoCaptureStreamSinkWinUWP::Shutdown() {
  CriticalSectionScoped cs(_critSec);

  if (!_isShutdown) {
    if (_spEventQueue) {
      _spEventQueue->Shutdown();
    }

    MFUnlockWorkQueue(_workQueueId);

    while (!_sampleQueue.empty()) {
      _sampleQueue.pop();
    }

    _spSink.Reset();
    _spEventQueue.Reset();
    _spByteStream.Reset();
    _spCurrentType.Reset();

    _isShutdown = true;
  }

  return S_OK;
}

// Puts an async operation on the work queue.
HRESULT VideoCaptureStreamSinkWinUWP::QueueAsyncOperation(StreamOperation op) {
  HRESULT hr = S_OK;
  ComPtr<AsyncOperation> spOp;
  spOp.Attach(new AsyncOperation(op));  // Created with ref count = 1
  if (!spOp) {
    hr = E_OUTOFMEMORY;
  }

  if (SUCCEEDED(hr)) {
    hr = MFPutWorkItem2(_workQueueId, 0, &_workQueueCB, spOp.Get());
  }

  if (!SUCCEEDED(hr)) {
    LOG_F(LS_ERROR) << "Capture stream sink error: " << hr;
  }

  return hr;
}

HRESULT VideoCaptureStreamSinkWinUWP::OnDispatchWorkItem(
    IMFAsyncResult *pAsyncResult) {
  try {
    ComPtr<IUnknown> spState;

    ThrowIfError(pAsyncResult->GetState(&spState));

    // The state object is a AsyncOperation object.
    AsyncOperation *pOp = static_cast<AsyncOperation *>(spState.Get());
    StreamOperation op = pOp->_op;

    switch (op) {
    case OpStart:
    case OpRestart:
      // Send MEStreamSinkStarted.
      ThrowIfError(QueueEvent(MEStreamSinkStarted, GUID_NULL, S_OK, nullptr));

      // There might be samples queue from earlier (ie, while paused).
      bool fRequestMoreSamples;
      fRequestMoreSamples = DropSamplesFromQueue();
      if (fRequestMoreSamples && !_isShutdown) {
        // If false there is no samples in the queue now so request one
        ThrowIfError(
          QueueEvent(MEStreamSinkRequestSample, GUID_NULL, S_OK, nullptr));
      }
      break;

    case OpStop:
      // Drop samples from queue.
      DropSamplesFromQueue();

      // Send the event even if the previous call failed.
      ThrowIfError(QueueEvent(MEStreamSinkStopped, GUID_NULL, S_OK, nullptr));
      break;

    case OpPause:
      ThrowIfError(QueueEvent(MEStreamSinkPaused, GUID_NULL, S_OK, nullptr));
      break;

    case OpProcessSample:
    case OpPlaceMarker:
    case OpSetMediaType:
      DispatchProcessSample(pOp);
      break;
    }
  } catch (Platform::Exception ^exc) {
    HandleError(exc->HResult);
  }
  return S_OK;
}

// Complete a ProcessSample request.
void VideoCaptureStreamSinkWinUWP::DispatchProcessSample(AsyncOperation *pOp) {
  assert(pOp != nullptr);
  bool fRequestMoreSamples = SendSampleFromQueue();

  // Ask for another sample
  if (fRequestMoreSamples && !_isShutdown) {
    if (pOp->_op == OpProcessSample) {
      ThrowIfError(
        QueueEvent(MEStreamSinkRequestSample, GUID_NULL, S_OK, nullptr));
    }
  }
}

// Drop samples in the queue
bool VideoCaptureStreamSinkWinUWP::DropSamplesFromQueue() {
  ProcessSamplesFromQueue(true);
  return true;
}

// Send sample from the queue
bool VideoCaptureStreamSinkWinUWP::SendSampleFromQueue() {
  return ProcessSamplesFromQueue(false);
}

bool VideoCaptureStreamSinkWinUWP::ProcessSamplesFromQueue(bool fFlush) {
  bool fNeedMoreSamples = false;

  ComPtr<IUnknown> spunkSample;

  bool fSendSamples = true;

  {
    CriticalSectionScoped cs(_critSec);

    if (_sampleQueue.size() == 0) {
      fNeedMoreSamples = true;
      fSendSamples = false;
    } else {
      spunkSample = _sampleQueue.front();
      _sampleQueue.pop();
    }
  }

  while (fSendSamples) {
    ComPtr<IMFSample> spSample;
    bool fProcessingSample = false;
    assert(spunkSample);

    if (SUCCEEDED(spunkSample.As(&spSample))) {
      assert(spSample);
      ComPtr<IMFMediaBuffer> spMediaBuffer;
      HRESULT hr = spSample->GetBufferByIndex(0, &spMediaBuffer);
      if (FAILED(hr)) {
        break;
      }

      _callback->OnSample(ref new MediaSampleEventArgs(spSample));
      if (!fFlush) {
        fProcessingSample = true;
      }
    } else {
      ComPtr<IMarker> spMarker;
      if (SUCCEEDED(spunkSample.As(&spMarker))) {
        MFSTREAMSINK_MARKER_TYPE markerType;
        PROPVARIANT var;
        PropVariantInit(&var);
        ThrowIfError(spMarker->GetMarkerType(&markerType));
        ThrowIfError(spMarker->GetContext(&var));

        HRESULT hr = QueueEvent(MEStreamSinkMarker, GUID_NULL, S_OK, &var);

        PropVariantClear(&var);

        ThrowIfError(hr);
      }
    }

    {
      CriticalSectionScoped cs(_critSec);

      if (_state == State_Started && fProcessingSample && !_isShutdown) {
        // If we are still in started state request another sample
        ThrowIfError(QueueEvent(MEStreamSinkRequestSample,
          GUID_NULL, S_OK, nullptr));
      }

      if (_sampleQueue.size() == 0) {
        fNeedMoreSamples = true;
        fSendSamples = false;
      } else {
        spunkSample = _sampleQueue.front();
        _sampleQueue.pop();
      }
    }
  }

  return fNeedMoreSamples;
}

// Processing format change
void VideoCaptureStreamSinkWinUWP::ProcessFormatChange(
    IMFMediaType *pMediaType) {
  assert(pMediaType != nullptr);

  // Add the media type to the sample queue.
  _sampleQueue.push(pMediaType);

  // Unless we are paused, start an async operation to dispatch the next sample.
  // Queue the operation.
  ThrowIfError(QueueAsyncOperation(OpSetMediaType));
}

VideoCaptureStreamSinkWinUWP::AsyncOperation::AsyncOperation(StreamOperation op)
  : _cRef(1),
    _op(op) {
}

VideoCaptureStreamSinkWinUWP::AsyncOperation::~AsyncOperation() {
  assert(_cRef == 0);
}

ULONG VideoCaptureStreamSinkWinUWP::AsyncOperation::AddRef() {
  return InterlockedIncrement(&_cRef);
}

ULONG VideoCaptureStreamSinkWinUWP::AsyncOperation::Release() {
  ULONG cRef = InterlockedDecrement(&_cRef);
  if (cRef == 0) {
    delete this;
  }

  return cRef;
}

HRESULT VideoCaptureStreamSinkWinUWP::AsyncOperation::QueryInterface(
    REFIID iid,
    void **ppv) {
  if (!ppv) {
    return E_POINTER;
  }
  if (iid == IID_IUnknown) {
    *ppv = static_cast<IUnknown*>(this);
  } else {
    *ppv = nullptr;
    return E_NOINTERFACE;
  }
  AddRef();
  return S_OK;
}

VideoCaptureStreamSinkWinUWP::Marker::Marker(
    MFSTREAMSINK_MARKER_TYPE eMarkerType)
  : _cRef(1),
    _eMarkerType(eMarkerType) {
  ZeroMemory(&_varMarkerValue, sizeof(_varMarkerValue));
  ZeroMemory(&_varContextValue, sizeof(_varContextValue));
}

VideoCaptureStreamSinkWinUWP::Marker::~Marker() {
  assert(_cRef == 0);

  PropVariantClear(&_varMarkerValue);
  PropVariantClear(&_varContextValue);
}

/* static */
HRESULT VideoCaptureStreamSinkWinUWP::Marker::Create(
    MFSTREAMSINK_MARKER_TYPE eMarkerType,
    const PROPVARIANT *pvarMarkerValue,
    const PROPVARIANT *pvarContextValue,
    IMarker **ppMarker) {

  if (ppMarker == nullptr) {
    return E_POINTER;
  }

  HRESULT hr = S_OK;
  ComPtr<Marker> spMarker;

  spMarker.Attach(new (std::nothrow) Marker(eMarkerType));

  if (spMarker == nullptr) {
    hr = E_OUTOFMEMORY;
  }

  if (SUCCEEDED(hr)) {
    if (pvarMarkerValue) {
      hr = PropVariantCopy(&spMarker->_varMarkerValue, pvarMarkerValue);
    }
  }

  if (SUCCEEDED(hr)) {
    if (pvarContextValue) {
      hr = PropVariantCopy(&spMarker->_varContextValue, pvarContextValue);
    }
  }

  if (SUCCEEDED(hr)) {
    *ppMarker = spMarker.Detach();
  }

  return hr;
}

// IUnknown methods.
IFACEMETHODIMP_(ULONG) VideoCaptureStreamSinkWinUWP::Marker::AddRef() {
  return InterlockedIncrement(&_cRef);
}

IFACEMETHODIMP_(ULONG) VideoCaptureStreamSinkWinUWP::Marker::Release() {

  ULONG cRef = InterlockedDecrement(&_cRef);
  if (cRef == 0) {
    delete this;
  }

  return cRef;
}

IFACEMETHODIMP VideoCaptureStreamSinkWinUWP::Marker::QueryInterface(
    REFIID riid, void **ppv) {

  if (ppv == nullptr) {
    return E_POINTER;
  }
  (*ppv) = nullptr;

  HRESULT hr = S_OK;
  if (riid == IID_IUnknown || riid == __uuidof(IMarker)) {
    (*ppv) = static_cast<IMarker*>(this);
    AddRef();
  } else {
    hr = E_NOINTERFACE;
  }

  return hr;
}

// IMarker methods
IFACEMETHODIMP VideoCaptureStreamSinkWinUWP::Marker::GetMarkerType(
    MFSTREAMSINK_MARKER_TYPE *pType) {

  if (pType == NULL) {
    return E_POINTER;
  }

  *pType = _eMarkerType;
  return S_OK;
}

IFACEMETHODIMP VideoCaptureStreamSinkWinUWP::Marker::GetMarkerValue(
    PROPVARIANT *pvar) {
  if (pvar == NULL) {
    return E_POINTER;
  }
  return PropVariantCopy(pvar, &_varMarkerValue);
}

IFACEMETHODIMP VideoCaptureStreamSinkWinUWP::Marker::GetContext(
    PROPVARIANT *pvar) {
  if (pvar == NULL) {
    return E_POINTER;
  }
  return PropVariantCopy(pvar, &_varContextValue);
}

void VideoCaptureStreamSinkWinUWP::HandleError(HRESULT hr) {
  if (!_isShutdown) {
    QueueEvent(MEError, GUID_NULL, hr, nullptr);
  }
}

VideoCaptureMediaSinkWinUWP::VideoCaptureMediaSinkWinUWP()
    : _cRef(1),
      _critSec(CriticalSectionWrapper::CreateCriticalSection()),
      _isShutdown(false),
      _isConnected(false),
      _llStartTime(0) {
}

VideoCaptureMediaSinkWinUWP::~VideoCaptureMediaSinkWinUWP() {
  assert(_isShutdown);
  if (_critSec) {
    delete _critSec;
  }
}

HRESULT VideoCaptureMediaSinkWinUWP::RuntimeClassInitialize(
    ISinkCallback ^callback,
    IMediaEncodingProperties ^encodingProperties) {
  try {
    _callback = callback;
    const unsigned int streamId = GetStreamId();
    RemoveStreamSink(streamId);
    if (encodingProperties != nullptr) {
      ComPtr<IMFStreamSink> spStreamSink;
      ComPtr<IMFMediaType> spMediaType;
      ConvertPropertiesToMediaType(encodingProperties, &spMediaType);
      ThrowIfError(AddStreamSink(streamId, spMediaType.Get(),
        spStreamSink.GetAddressOf()));
    }
  } catch (Platform::Exception ^exc) {
    _callback = nullptr;
    return exc->HResult;
  }

  return S_OK;
}

///  IMFMediaSink
IFACEMETHODIMP VideoCaptureMediaSinkWinUWP::GetCharacteristics(
    DWORD *pdwCharacteristics) {
  if (pdwCharacteristics == NULL) {
    return E_INVALIDARG;
  }
  CriticalSectionScoped cs(_critSec);

  HRESULT hr;
  if (_isShutdown) {
    hr = MF_E_SHUTDOWN;
  } else {
    hr = S_OK;
  }

  if (SUCCEEDED(hr)) {
    // Rateless sink.
    *pdwCharacteristics = MEDIASINK_RATELESS;
  } else {
    LOG_F(LS_ERROR) << "Capture media sink error: " << hr;
  }

  return hr;
}

IFACEMETHODIMP VideoCaptureMediaSinkWinUWP::AddStreamSink(
    DWORD dwStreamSinkIdentifier,
    IMFMediaType *pMediaType,
    IMFStreamSink **ppStreamSink) {
  VideoCaptureStreamSinkWinUWP *pStream = nullptr;
  ComPtr<IMFStreamSink> spMFStream;
  CriticalSectionScoped cs(_critSec);
  HRESULT hr = S_OK;
  if (_isShutdown) {
    hr = MF_E_SHUTDOWN;
  }

  if (SUCCEEDED(hr) && dwStreamSinkIdentifier != GetStreamId()) {
    hr = MF_E_INVALIDSTREAMNUMBER;
  }

  if (SUCCEEDED(hr)) {
    hr = GetStreamSinkById(dwStreamSinkIdentifier, &spMFStream);
  }

  if (SUCCEEDED(hr)) {
    hr = MF_E_STREAMSINK_EXISTS;
  } else {
    hr = S_OK;
  }

  if (SUCCEEDED(hr)) {
    pStream = new VideoCaptureStreamSinkWinUWP(dwStreamSinkIdentifier);
    if (pStream == nullptr) {
      hr = E_OUTOFMEMORY;
    }
    spMFStream.Attach(pStream);
  }

  // Initialize the stream.
  if (SUCCEEDED(hr)) {
    hr = pStream->Initialize(this, _callback);
  }

  if (SUCCEEDED(hr) && pMediaType != nullptr) {
    hr = pStream->SetCurrentMediaType(pMediaType);
  }

  if (SUCCEEDED(hr)) {
    _spStreamSink = spMFStream;
    *ppStreamSink = spMFStream.Detach();
  } else {
    LOG_F(LS_ERROR) << "Capture media sink error: " << hr;
  }

  return hr;
}

IFACEMETHODIMP VideoCaptureMediaSinkWinUWP::RemoveStreamSink(
    DWORD dwStreamSinkIdentifier) {
  CriticalSectionScoped cs(_critSec);
  HRESULT hr = S_OK;
  if (_isShutdown) {
    hr = MF_E_SHUTDOWN;
  }

  if (SUCCEEDED(hr) && dwStreamSinkIdentifier != GetStreamId()) {
    hr = MF_E_INVALIDSTREAMNUMBER;
  }

  if (SUCCEEDED(hr) && _spStreamSink) {
    ComPtr<IMFStreamSink> spStream = _spStreamSink;
    static_cast<VideoCaptureStreamSinkWinUWP *>(spStream.Get())->Shutdown();
  }

  if (!SUCCEEDED(hr)) {
    LOG_F(LS_ERROR) << "Capture media sink error: " << hr;
  }

  return hr;
}

IFACEMETHODIMP VideoCaptureMediaSinkWinUWP::GetStreamSinkCount(
    _Out_ DWORD *pcStreamSinkCount) {
  if (pcStreamSinkCount == NULL) {
    return E_INVALIDARG;
  }

  CriticalSectionScoped cs(_critSec);

  HRESULT hr = S_OK;
  if (_isShutdown) {
    hr = MF_E_SHUTDOWN;
  }

  if (SUCCEEDED(hr)) {
    *pcStreamSinkCount = 1;
  } else {
    LOG_F(LS_ERROR) << "Capture media sink error: " << hr;
  }

  return hr;
}

IFACEMETHODIMP VideoCaptureMediaSinkWinUWP::GetStreamSinkByIndex(
    DWORD dwIndex,
    _Outptr_ IMFStreamSink **ppStreamSink) {
  if (ppStreamSink == NULL) {
    return E_INVALIDARG;
  }

  CriticalSectionScoped cs(_critSec);

  if (dwIndex >= 1) {
    return MF_E_INVALIDINDEX;
  }

  HRESULT hr = S_OK;
  if (_isShutdown) {
    hr = MF_E_SHUTDOWN;
  }

  if (SUCCEEDED(hr)) {
    assert(_spStreamSink);
    ComPtr<IMFStreamSink> spResult = _spStreamSink;
    *ppStreamSink = spResult.Detach();
  } else {
    LOG_F(LS_ERROR) << "Capture media sink error: " << hr;
  }

  return hr;
}

IFACEMETHODIMP VideoCaptureMediaSinkWinUWP::GetStreamSinkById(
    DWORD dwStreamSinkIdentifier,
    IMFStreamSink **ppStreamSink) {
  if (ppStreamSink == NULL) {
    return E_INVALIDARG;
  }

  CriticalSectionScoped cs(_critSec);
  HRESULT hr = S_OK;
  if (_isShutdown) {
    hr = MF_E_SHUTDOWN;
  }

  if (dwStreamSinkIdentifier != GetStreamId() || _spStreamSink == nullptr) {
    hr = MF_E_INVALIDSTREAMNUMBER;
  }

  if (SUCCEEDED(hr)) {
    assert(_spStreamSink);
    ComPtr<IMFStreamSink> spResult = _spStreamSink;
    *ppStreamSink = spResult.Detach();
  }

  return hr;
}

IFACEMETHODIMP VideoCaptureMediaSinkWinUWP::SetPresentationClock(
    IMFPresentationClock *pPresentationClock) {
  CriticalSectionScoped cs(_critSec);

  HRESULT hr = S_OK;
  if (_isShutdown) {
    hr = MF_E_SHUTDOWN;
  }

  // If we already have a clock, remove ourselves from that clock's
  // state notifications.
  if (SUCCEEDED(hr)) {
    if (_spClock) {
      hr = _spClock->RemoveClockStateSink(this);
    }
  }

  // Register ourselves to get state notifications from the new clock.
  if (SUCCEEDED(hr)) {
    if (pPresentationClock) {
      hr = pPresentationClock->AddClockStateSink(this);
    }
  }

  if (SUCCEEDED(hr)) {
    // Release the pointer to the old clock.
    // Store the pointer to the new clock.
    _spClock = pPresentationClock;
  } else {
    LOG_F(LS_ERROR) << "Capture media sink error: " << hr;
  }

  return hr;
}

IFACEMETHODIMP VideoCaptureMediaSinkWinUWP::GetPresentationClock(
    IMFPresentationClock **ppPresentationClock) {
  if (ppPresentationClock == NULL) {
    return E_INVALIDARG;
  }

  CriticalSectionScoped cs(_critSec);

  HRESULT hr = S_OK;
  if (_isShutdown) {
    hr = MF_E_SHUTDOWN;
  }

  if (SUCCEEDED(hr)) {
    if (_spClock == NULL) {
      hr = MF_E_NO_CLOCK;  // There is no presentation clock.
    } else {
      // Return the pointer to the caller.
      *ppPresentationClock = _spClock.Get();
      (*ppPresentationClock)->AddRef();
    }
  }

  if (!SUCCEEDED(hr)) {
    LOG_F(LS_ERROR) << "Capture media sink error: " << hr;
  }

  return hr;
}

IFACEMETHODIMP VideoCaptureMediaSinkWinUWP::Shutdown() {
  ISinkCallback ^callback;
  {
    CriticalSectionScoped cs(_critSec);
    HRESULT hr = S_OK;
    if (_isShutdown) {
      hr = MF_E_SHUTDOWN;
    }

    if (SUCCEEDED(hr)) {
      ComPtr<IMFStreamSink> spMFStream = _spStreamSink;
      _spClock.Reset();
      static_cast<VideoCaptureStreamSinkWinUWP *>(spMFStream.Get())->Shutdown();
      _isShutdown = true;
      callback = _callback;
    }
  }

  if (callback != nullptr) {
    callback->OnShutdown();
  }

  return S_OK;
}

// IMFClockStateSink
IFACEMETHODIMP VideoCaptureMediaSinkWinUWP::OnClockStart(
    MFTIME hnsSystemTime,
    LONGLONG llClockStartOffset) {
  CriticalSectionScoped cs(_critSec);

  HRESULT hr = S_OK;
  if (_isShutdown) {
    hr = MF_E_SHUTDOWN;
  }

  if (SUCCEEDED(hr)) {
    _llStartTime = llClockStartOffset;
    static_cast<VideoCaptureStreamSinkWinUWP *>(_spStreamSink.Get())->Start(
      _llStartTime);
  } else {
    LOG_F(LS_ERROR) << "Capture media sink error: " << hr;
  }

  return hr;
}

IFACEMETHODIMP VideoCaptureMediaSinkWinUWP::OnClockStop(
    MFTIME hnsSystemTime) {
  CriticalSectionScoped cs(_critSec);

  HRESULT hr = S_OK;
  if (_isShutdown) {
    hr = MF_E_SHUTDOWN;
  }

  if (SUCCEEDED(hr)) {
    static_cast<VideoCaptureStreamSinkWinUWP *>(_spStreamSink.Get())->Stop();
  } else {
    LOG_F(LS_ERROR) << "Capture media sink error: " << hr;
  }

  return hr;
}

IFACEMETHODIMP VideoCaptureMediaSinkWinUWP::OnClockPause(
    MFTIME hnsSystemTime) {
  return MF_E_INVALID_STATE_TRANSITION;
}

IFACEMETHODIMP VideoCaptureMediaSinkWinUWP::OnClockRestart(
    MFTIME hnsSystemTime) {
  return MF_E_INVALID_STATE_TRANSITION;
}

IFACEMETHODIMP VideoCaptureMediaSinkWinUWP::OnClockSetRate(
    /* [in] */ MFTIME hnsSystemTime,
    /* [in] */ float flRate) {
  return S_OK;
}

VideoCaptureMediaSinkProxyWinUWP::VideoCaptureMediaSinkProxyWinUWP()
  : _critSec(CriticalSectionWrapper::CreateCriticalSection()) {
}

VideoCaptureMediaSinkProxyWinUWP::~VideoCaptureMediaSinkProxyWinUWP() {
  if (_mediaSink != nullptr) {
    _mediaSink->Shutdown();
    _mediaSink = nullptr;
  }

  if (_critSec) {
    delete _critSec;
  }
}

IMediaExtension^ VideoCaptureMediaSinkProxyWinUWP::GetMFExtension() {
  CriticalSectionScoped cs(_critSec);

  if (_mediaSink == nullptr) {
    Throw(MF_E_NOT_INITIALIZED);
  }

  ComPtr<IInspectable> inspectable;
  ThrowIfError(_mediaSink.As(&inspectable));

  return safe_cast<IMediaExtension^>(reinterpret_cast<Object^>(
    inspectable.Get()));
}


Windows::Foundation::IAsyncOperation<IMediaExtension^>^
    VideoCaptureMediaSinkProxyWinUWP::InitializeAsync(
        IMediaEncodingProperties ^encodingProperties) {
  return Concurrency::create_async([this, encodingProperties]() {
    CriticalSectionScoped cs(_critSec);
    CheckShutdown();

    if (_mediaSink != nullptr) {
      Throw(MF_E_ALREADY_INITIALIZED);
    }

    // Prepare the MF extension
    ThrowIfError(Microsoft::WRL::MakeAndInitialize<VideoCaptureMediaSinkWinUWP>(
        &_mediaSink,
        ref new VideoCaptureSinkCallback(this),
        encodingProperties));

    ComPtr<IInspectable> inspectable;
    ThrowIfError(_mediaSink.As(&inspectable));

    return safe_cast<IMediaExtension^>(reinterpret_cast<Object^>(
      inspectable.Get()));
  });
}

void VideoCaptureMediaSinkProxyWinUWP::OnSample(MediaSampleEventArgs^ args) {
  MediaSampleEvent(this, args);
}

void VideoCaptureMediaSinkProxyWinUWP::OnShutdown() {
  CriticalSectionScoped cs(_critSec);
  if (_shutdown) {
    return;
  }
  _shutdown = true;
  _mediaSink = nullptr;
}

void VideoCaptureMediaSinkProxyWinUWP::CheckShutdown() {
  if (_shutdown) {
    Throw(MF_E_SHUTDOWN);
  }
}

}  // namespace videocapturemodule
}  // namespace webrtc
