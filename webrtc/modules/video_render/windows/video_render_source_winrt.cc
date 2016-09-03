/*
*  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
*
*  Use of this source code is governed by a BSD-style license
*  that can be found in the LICENSE file in the root of the source
*  tree. An additional intellectual property rights grant can be found
*  in the file PATENTS.  All contributing project authors may
*  be found in the AUTHORS file in the root of the source tree.
*/

#include "webrtc/modules/video_render/windows/video_render_source_winrt.h"

#include <wrl/module.h>

#include <strsafe.h>

#include <mferror.h>
#include <mfapi.h>

#include <assert.h>
#include <ppltasks.h>

#include "webrtc/system_wrappers/include/critical_section_wrapper.h"
#include "webrtc/system_wrappers/include/logging.h"


using Microsoft::WRL::ComPtr;

namespace webrtc {

inline void ThrowIfError(HRESULT hr) {
  if (FAILED(hr)) {
    throw ref new Platform::Exception(hr);
  }
}

inline void Throw(HRESULT hr) {
  assert(FAILED(hr));
  throw ref new Platform::Exception(hr);
}

// RAII object locking the the source on initialization and unlocks it on
// deletion
class VideoRenderMediaStreamWinRT::SourceLock {
 public:
  explicit SourceLock(VideoRenderMediaSourceWinRT *pSource)
    : _spSource(pSource) {
    if (_spSource) {
      _spSource->Lock();
    }
  }

  ~SourceLock() {
    if (_spSource) {
      _spSource->Unlock();
    }
  }

 private:
  ComPtr<VideoRenderMediaSourceWinRT> _spSource;
};

HRESULT VideoRenderMediaStreamWinRT::CreateInstance(
  StreamDescription *pStreamDescription, VideoRenderMediaSourceWinRT *pSource,
  VideoRenderMediaStreamWinRT **ppStream) {
  if (pSource == nullptr || ppStream == nullptr) {
    return E_INVALIDARG;
  }

  HRESULT hr = S_OK;
  try {
    ComPtr<VideoRenderMediaStreamWinRT> spStream;
    spStream.Attach(new (std::nothrow) VideoRenderMediaStreamWinRT(pSource));
    if (!spStream) {
      Throw(E_OUTOFMEMORY);
    }

    spStream->Initialize(pStreamDescription);

    (*ppStream) = spStream.Detach();
  } catch (Platform::Exception ^exc) {
    hr = exc->HResult;
  }

  if (!SUCCEEDED(hr)) {
    LOG_F(LS_ERROR) << "Render media stream error: " << hr;
  }

  return hr;
}

VideoRenderMediaStreamWinRT::VideoRenderMediaStreamWinRT(
  VideoRenderMediaSourceWinRT *pSource)
  : _cRef(1),
    _spSource(pSource),
    _eSourceState(SourceState_Invalid),
    _fActive(false),
    _flRate(1.0f),
    _eDropMode(MF_DROP_MODE_NONE),
    _fDiscontinuity(false),
    _fDropTime(false),
    _fInitDropTime(false),
    _fWaitingForCleanPoint(true),
    _hnsStartDroppingAt(0),
    _hnsAmountToDrop(0) {
}

VideoRenderMediaStreamWinRT::~VideoRenderMediaStreamWinRT(void) {
}

// IUnknown methods
IFACEMETHODIMP VideoRenderMediaStreamWinRT::QueryInterface(REFIID riid,
  void **ppv) {
  if (ppv == nullptr) {
    return E_POINTER;
  }
  (*ppv) = nullptr;

  HRESULT hr = S_OK;
  if (riid == IID_IUnknown ||
    riid == IID_IMFMediaStream ||
    riid == IID_IMFMediaEventGenerator) {
    (*ppv) = static_cast<IMFMediaStream*>(this);
    AddRef();
  } else if (riid == IID_IMFQualityAdvise || riid == IID_IMFQualityAdvise2) {
    (*ppv) = static_cast<IMFQualityAdvise2*>(this);
    AddRef();
  } else if (riid == IID_IMFGetService) {
    (*ppv) = static_cast<IMFGetService*>(this);
    AddRef();
  } else {
    hr = E_NOINTERFACE;
  }

  return hr;
}

IFACEMETHODIMP_(ULONG) VideoRenderMediaStreamWinRT::AddRef() {
  return InterlockedIncrement(&_cRef);
}

IFACEMETHODIMP_(ULONG) VideoRenderMediaStreamWinRT::Release() {
  long cRef = InterlockedDecrement(&_cRef);
  if (cRef == 0) {
    delete this;
  }
  return cRef;
}

// IMFMediaEventGenerator methods.
IFACEMETHODIMP VideoRenderMediaStreamWinRT::BeginGetEvent(
  IMFAsyncCallback *pCallback, IUnknown *punkState) {
  HRESULT hr = S_OK;

  SourceLock lock(_spSource.Get());

  if (_eSourceState == SourceState_Shutdown) {
    hr = MF_E_SHUTDOWN;
  }

  if (SUCCEEDED(hr)) {
    hr = _spEventQueue->BeginGetEvent(pCallback, punkState);
  }

  if (!SUCCEEDED(hr)) {
    LOG_F(LS_ERROR) << "Render media stream error: " << hr;
  }

  return hr;
}

IFACEMETHODIMP VideoRenderMediaStreamWinRT::EndGetEvent(
  IMFAsyncResult *pResult, IMFMediaEvent **ppEvent) {
  HRESULT hr = S_OK;

  SourceLock lock(_spSource.Get());

  if (_eSourceState == SourceState_Shutdown) {
    hr = MF_E_SHUTDOWN;
  }

  if (SUCCEEDED(hr)) {
    hr = _spEventQueue->EndGetEvent(pResult, ppEvent);
  }

  if (!SUCCEEDED(hr)) {
    LOG_F(LS_ERROR) << "Render media stream error: " << hr;
  }

  return hr;
}

IFACEMETHODIMP VideoRenderMediaStreamWinRT::GetEvent(DWORD dwFlags,
  IMFMediaEvent **ppEvent) {
  // NOTE:
  // GetEvent can block indefinitely, so we don't hold the lock.
  // This requires some juggling with the event queue pointer.

  HRESULT hr = S_OK;

  ComPtr<IMFMediaEventQueue> spQueue;

  {
    SourceLock lock(_spSource.Get());

    // Check shutdown
    if (_eSourceState == SourceState_Shutdown) {
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
    LOG_F(LS_ERROR) << "Render media stream error: " << hr;
  }

  return hr;
}

IFACEMETHODIMP VideoRenderMediaStreamWinRT::QueueEvent(MediaEventType met,
  REFGUID guidExtendedType, HRESULT hrStatus, PROPVARIANT const *pvValue) {
  HRESULT hr = S_OK;

  SourceLock lock(_spSource.Get());

  if (_eSourceState == SourceState_Shutdown) {
    hr = MF_E_SHUTDOWN;
  }

  if (SUCCEEDED(hr)) {
    hr = _spEventQueue->QueueEventParamVar(met, guidExtendedType, hrStatus,
      pvValue);
  }

  return hr;
}

// IMFMediaStream methods
IFACEMETHODIMP VideoRenderMediaStreamWinRT::GetMediaSource(
  IMFMediaSource **ppMediaSource) {
  if (ppMediaSource == nullptr) {
    return E_INVALIDARG;
  }

  HRESULT hr = S_OK;

  SourceLock lock(_spSource.Get());

  if (_eSourceState == SourceState_Shutdown) {
    hr = MF_E_SHUTDOWN;
  }

  if (SUCCEEDED(hr)) {
    *ppMediaSource = _spSource.Get();
    (*ppMediaSource)->AddRef();
  } else {
    LOG_F(LS_ERROR) << "Render media stream error: " << hr;
  }

  return hr;
}

IFACEMETHODIMP VideoRenderMediaStreamWinRT::GetStreamDescriptor(
  IMFStreamDescriptor **ppStreamDescriptor) {
  if (ppStreamDescriptor == nullptr) {
    return E_INVALIDARG;
  }

  HRESULT hr = S_OK;

  SourceLock lock(_spSource.Get());

  if (_eSourceState == SourceState_Shutdown) {
    hr = MF_E_SHUTDOWN;
  }

  if (SUCCEEDED(hr)) {
    *ppStreamDescriptor = _spStreamDescriptor.Get();
    (*ppStreamDescriptor)->AddRef();
  } else {
    LOG_F(LS_ERROR) << "Render media stream error: " << hr;
  }

  return hr;
}

IFACEMETHODIMP VideoRenderMediaStreamWinRT::RequestSample(IUnknown *pToken) {
  SourceLock lock(_spSource.Get());

  try {
    if (_eSourceState == SourceState_Shutdown) {
      Throw(MF_E_SHUTDOWN);
    }

    if (_eSourceState != SourceState_Started) {
      // We cannot be asked for a sample unless we are in started state
      Throw(MF_E_INVALIDREQUEST);
    }

    // Put token onto the list to return it when we have a sample ready
    _tokens.push_back(pToken);

    // Trigger sample delivery
    DeliverSamples();
  } catch (Platform::Exception ^exc) {
    HandleError(exc->HResult);
  }

  return S_OK;
}

// IMFQualityAdvise methods
IFACEMETHODIMP VideoRenderMediaStreamWinRT::SetDropMode(
  _In_ MF_QUALITY_DROP_MODE eDropMode) {
  HRESULT hr = S_OK;
  SourceLock lock(_spSource.Get());

  if (_eSourceState == SourceState_Shutdown) {
    hr = MF_E_SHUTDOWN;
  }

  //
  // Only support one drop mode
  //
  if (SUCCEEDED(hr) &&
    ((eDropMode < MF_DROP_MODE_NONE) || (eDropMode >= MF_DROP_MODE_2))) {
    hr = MF_E_NO_MORE_DROP_MODES;
  }

  if (SUCCEEDED(hr) && _eDropMode != eDropMode) {
    _eDropMode = eDropMode;
    _fWaitingForCleanPoint = true;
    LOG(LS_INFO) << "Setting drop mode to " << _eDropMode;
  }

  if (!SUCCEEDED(hr)) {
    LOG_F(LS_ERROR) << "Render media stream error: " << hr;
  }

  return hr;
}

IFACEMETHODIMP VideoRenderMediaStreamWinRT::SetQualityLevel(
  _In_ MF_QUALITY_LEVEL eQualityLevel) {
  return(MF_E_NO_MORE_QUALITY_LEVELS);
}

IFACEMETHODIMP VideoRenderMediaStreamWinRT::GetDropMode(
  _Out_  MF_QUALITY_DROP_MODE *peDropMode) {
  HRESULT hr = S_OK;

  if (peDropMode == nullptr) {
    return E_POINTER;
  }

  SourceLock lock(_spSource.Get());

  if (_eSourceState == SourceState_Shutdown) {
    hr = MF_E_SHUTDOWN;
  }

  if (SUCCEEDED(hr)) {
    *peDropMode = _eDropMode;
  } else {
    LOG_F(LS_ERROR) << "Render media stream error: " << hr;
  }

  return hr;
}

IFACEMETHODIMP VideoRenderMediaStreamWinRT::GetQualityLevel(
  _Out_  MF_QUALITY_LEVEL *peQualityLevel) {
  return(E_NOTIMPL);
}

IFACEMETHODIMP VideoRenderMediaStreamWinRT::DropTime(
  _In_ LONGLONG hnsAmountToDrop) {
  HRESULT hr = S_OK;
  SourceLock lock(_spSource.Get());

  if (_eSourceState == SourceState_Shutdown) {
    hr = MF_E_SHUTDOWN;
  }

  if (SUCCEEDED(hr)) {
    if (hnsAmountToDrop > 0) {
      _fDropTime = true;
      _fInitDropTime = true;
      _hnsAmountToDrop = hnsAmountToDrop;
      LOG(LS_INFO) << "Dropping time hnsAmountToDrop=" << hnsAmountToDrop;
    } else if (hnsAmountToDrop == 0) {
      // Reset time dropping
      LOG(LS_INFO) << "Disabling dropping time";
      ResetDropTime();
    } else {
      hr = E_INVALIDARG;
    }
  }

  if (!SUCCEEDED(hr)) {
    LOG_F(LS_ERROR) << "Render media stream error: " << hr;
  }

  return hr;
}

// IMFQualityAdvise2 methods
IFACEMETHODIMP VideoRenderMediaStreamWinRT::NotifyQualityEvent(
  _In_opt_ IMFMediaEvent *pEvent, _Out_ DWORD *pdwFlags) {
  HRESULT hr = S_OK;
  SourceLock lock(_spSource.Get());

  if (pdwFlags == nullptr || pEvent == nullptr) {
    return E_POINTER;
  }

  *pdwFlags = 0;

  if (_eSourceState == SourceState_Shutdown) {
    hr = MF_E_SHUTDOWN;
  }

  if (SUCCEEDED(hr)) {
    MediaEventType met;
    hr = pEvent->GetType(&met);

    if (SUCCEEDED(hr) && met == MEQualityNotify) {
      GUID guiExtendedType;
      hr = pEvent->GetExtendedType(&guiExtendedType);

      if (SUCCEEDED(hr) && guiExtendedType == MF_QUALITY_NOTIFY_SAMPLE_LAG) {
        PROPVARIANT var;
        PropVariantInit(&var);

        hr = pEvent->GetValue(&var);
        LONGLONG hnsSampleLatency = var.hVal.QuadPart;

        if (SUCCEEDED(hr)) {
          if (_eDropMode == MF_DROP_MODE_NONE && hnsSampleLatency > 30000000) {
            hr = SetDropMode(MF_DROP_MODE_1);
            LOG(LS_INFO) << "Entering drop mode";
          } else if (_eDropMode == MF_DROP_MODE_1 && hnsSampleLatency < 0) {
            hr = SetDropMode(MF_DROP_MODE_NONE);
            LOG(LS_INFO) << "Leaving drop mode";
          } else {
            LOG(LS_INFO) << "Sample latency = " << hnsSampleLatency;
          }
        }

        PropVariantClear(&var);
      }
    }
  }

  if (!SUCCEEDED(hr)) {
    LOG_F(LS_ERROR) << "Render media stream error: " << hr;
  }

  return hr;
}

// IMFGetService methods
IFACEMETHODIMP VideoRenderMediaStreamWinRT::GetService(
  _In_ REFGUID guidService,
  _In_ REFIID riid,
  _Out_ LPVOID *ppvObject) {
  HRESULT hr = S_OK;
  SourceLock lock(_spSource.Get());

  if (ppvObject == nullptr) {
    return E_POINTER;
  }
  *ppvObject = NULL;

  if (_eSourceState == SourceState_Shutdown) {
    hr = MF_E_SHUTDOWN;
  }

  if (SUCCEEDED(hr)) {
    if (MF_QUALITY_SERVICES == guidService) {
      hr = QueryInterface(riid, ppvObject);
    } else {
      hr = MF_E_UNSUPPORTED_SERVICE;
    }
  }

  return hr;
}

HRESULT VideoRenderMediaStreamWinRT::Start() {
  HRESULT hr = S_OK;
  SourceLock lock(_spSource.Get());

  if (_eSourceState == SourceState_Shutdown) {
    hr = MF_E_SHUTDOWN;
  }

  if (SUCCEEDED(hr)) {
    if (_eSourceState == SourceState_Stopped ||
      _eSourceState == SourceState_Started) {
      _eSourceState = SourceState_Started;
      // Inform the client that we've started
      hr = QueueEvent(MEStreamStarted, GUID_NULL, S_OK, nullptr);
    } else {
      hr = MF_E_INVALID_STATE_TRANSITION;
    }
  }

  if (FAILED(hr)) {
    HandleError(hr);
  }

  if (!SUCCEEDED(hr)) {
    LOG_F(LS_ERROR) << "Render media stream error: " << hr;
  }

  return S_OK;
}

HRESULT VideoRenderMediaStreamWinRT::Stop() {
  HRESULT hr = S_OK;
  SourceLock lock(_spSource.Get());

  if (_eSourceState == SourceState_Shutdown) {
    hr = MF_E_SHUTDOWN;
  }

  if (SUCCEEDED(hr)) {
    if (_eSourceState == SourceState_Started) {
      _eSourceState = SourceState_Stopped;
      while (!_tokens.empty())
        _tokens.pop_front();
      while (!_samples.empty())
        _samples.pop_front();
      // Inform the client that we've stopped.
      hr = QueueEvent(MEStreamStopped, GUID_NULL, S_OK, nullptr);
    } else {
      hr = MF_E_INVALID_STATE_TRANSITION;
    }
  }

  if (FAILED(hr)) {
    HandleError(hr);
  }

  if (!SUCCEEDED(hr)) {
    LOG_F(LS_ERROR) << "Render media stream error: " << hr;
  }

  return S_OK;
}

HRESULT VideoRenderMediaStreamWinRT::SetRate(float flRate) {
  SourceLock lock(_spSource.Get());

  try {
    if (_eSourceState == SourceState_Shutdown) {
      Throw(MF_E_SHUTDOWN);
    }

    _flRate = flRate;
    if (_flRate != 1.0f) {
      CleanSampleQueue();
    }
  } catch (Platform::Exception ^exc) {
    HandleError(exc->HResult);
  }

  return S_OK;
}

HRESULT VideoRenderMediaStreamWinRT::Flush() {
  SourceLock lock(_spSource.Get());

  while (!_tokens.empty())
    _tokens.pop_front();
  while (!_samples.empty())
    _samples.pop_front();

  _fDiscontinuity = false;
  _eDropMode = MF_DROP_MODE_NONE;
  ResetDropTime();

  return S_OK;
}

HRESULT VideoRenderMediaStreamWinRT::Shutdown() {
  SourceLock lock(_spSource.Get());

  HRESULT hr = S_OK;
  if (_eSourceState == SourceState_Shutdown) {
    hr = MF_E_SHUTDOWN;
  }

  if (SUCCEEDED(hr)) {
    Flush();

    if (_spEventQueue) {
      _spEventQueue->Shutdown();
      _spEventQueue.Reset();
    }

    _spStreamDescriptor.Reset();
    _eSourceState = SourceState_Shutdown;
  }

  if (!SUCCEEDED(hr)) {
    LOG_F(LS_ERROR) << "Render media stream error: " << hr;
  }

  return hr;
}

// Processes media sample
void VideoRenderMediaStreamWinRT::ProcessSample(
  SampleHeader *pSampleHeader, IMFSample *pSample) {
  assert(pSample);

  try {
    if (_eSourceState == SourceState_Shutdown) {
      Throw(MF_E_SHUTDOWN);
    }
    // Set sample attributes
    SetSampleAttributes(pSampleHeader, pSample);

    // Check if we are in propper state if so deliver the sample otherwise just
    // skip it and don't treat it as an error.
    if (_eSourceState == SourceState_Started) {
      // Put sample on the list
      _samples.push_back(pSample);
      // Deliver samples
      DeliverSamples();
    } else {
      Throw(MF_E_UNEXPECTED);
    }
  } catch (Platform::Exception ^exc) {
    HandleError(exc->HResult);
  }
}

void VideoRenderMediaStreamWinRT::ProcessFormatChange(
  StreamDescription *pStreamDescription) {
  assert(pStreamDescription);

  _currentStreamDescription = *pStreamDescription;

  try {
    if (_eSourceState == SourceState_Shutdown) {
      Throw(MF_E_SHUTDOWN);
    }

    ComPtr<IMFMediaType> spMediaType;
    ComPtr<IMFMediaTypeHandler> spMediaTypeHandler;

    // Create a media type object.
    ThrowIfError(MFCreateMediaType(&spMediaType));
    SetMediaTypeAttributes(pStreamDescription, spMediaType.Get());

    ThrowIfError(_spStreamDescriptor->GetMediaTypeHandler(
      &spMediaTypeHandler));

    // Set current media type
    ThrowIfError(spMediaTypeHandler->SetCurrentMediaType(spMediaType.Get()));
  } catch (Platform::Exception ^exc) {
    HandleError(exc->HResult);
  }
}

HRESULT VideoRenderMediaStreamWinRT::SetActive(bool fActive) {
  SourceLock lock(_spSource.Get());

  HRESULT hr = S_OK;
  if (_eSourceState == SourceState_Shutdown) {
    hr = MF_E_SHUTDOWN;
  }

  if (SUCCEEDED(hr)) {
    if (_eSourceState != SourceState_Stopped &&
      _eSourceState != SourceState_Started) {
      hr = MF_E_INVALIDREQUEST;
    }
  }

  if (SUCCEEDED(hr)) {
    _fActive = fActive;
  } else {
    LOG_F(LS_ERROR) << "Render media stream error: " << hr;
  }


  return hr;
}

void VideoRenderMediaStreamWinRT::Initialize(
  StreamDescription *pStreamDescription) {
  // Create the media event queue.
  Platform::Exception ^error = nullptr;

  try {
    ThrowIfError(MFCreateEventQueue(&_spEventQueue));
    ComPtr<IMFMediaType> spMediaType;
    ComPtr<IMFStreamDescriptor> spSD;
    ComPtr<IMFMediaTypeHandler> spMediaTypeHandler;

    // Create a media type object.
    ThrowIfError(MFCreateMediaType(&spMediaType));
    SetMediaTypeAttributes(pStreamDescription, spMediaType.Get());

    // Now we can create MF stream descriptor.
    ThrowIfError(MFCreateStreamDescriptor(pStreamDescription->dwStreamId, 1,
      spMediaType.GetAddressOf(), &spSD));
    ThrowIfError(spSD->GetMediaTypeHandler(&spMediaTypeHandler));

    // Set current media type
    ThrowIfError(spMediaTypeHandler->SetCurrentMediaType(spMediaType.Get()));

    _spStreamDescriptor = spSD;
    _dwId = pStreamDescription->dwStreamId;
    // State of the stream is started.
    _eSourceState = SourceState_Stopped;

    _currentStreamDescription = *pStreamDescription;
  } catch (Platform::Exception ^exc) {
    error = exc;
  }

  if (error != nullptr) {
    throw error;
  }
}

// Deliver samples for every request client made
void VideoRenderMediaStreamWinRT::DeliverSamples() {
  // Check if we have both: samples available in the queue and requests in
  // request list.
  while (!_samples.empty() && !_tokens.empty()) {
    ComPtr<IUnknown> spEntry;
    ComPtr<IMFSample> spSample;
    ComPtr<IUnknown> spToken;
    BOOL fDrop = FALSE;
    // Get the entry
    spEntry = _samples.front();
    _samples.pop_front();

    if (SUCCEEDED(spEntry.As(&spSample))) {
      DWORD pcbTotalLength;
      LONGLONG phnsSampleTime;
      LONGLONG phnsSampleDuration;
      spSample->GetTotalLength(&pcbTotalLength);
      spSample->GetSampleTime(&phnsSampleTime);
      spSample->GetSampleDuration(&phnsSampleDuration);
      fDrop = ShouldDropSample(spSample.Get());

      if (!fDrop) {
        // Get the request token
        spToken = _tokens.front();
        _tokens.pop_front();

        if (spToken) {
          // If token was not null set the sample attribute.
          ThrowIfError(spSample->SetUnknown(MFSampleExtension_Token,
            spToken.Get()));
        }

        if (_fDiscontinuity) {
          // If token was not null set the sample attribute.
          ThrowIfError(spSample->SetUINT32(MFSampleExtension_Discontinuity,
            TRUE));
          _fDiscontinuity = false;
        }

        // Send a sample event.
        ThrowIfError(_spEventQueue->QueueEventParamUnk(MEMediaSample,
          GUID_NULL, S_OK, spSample.Get()));
      } else {
        _fDiscontinuity = true;
      }
    } else {
      ComPtr<IMFMediaType> spMediaType;
      ThrowIfError(spEntry.As(&spMediaType));
      // Send a sample event.
      ThrowIfError(_spEventQueue->QueueEventParamUnk(MEStreamFormatChanged,
        GUID_NULL, S_OK, spMediaType.Get()));
    }
  }
}

void VideoRenderMediaStreamWinRT::HandleError(HRESULT hErrorCode) {
  if (hErrorCode != MF_E_SHUTDOWN) {
    // Send MEError to the client
    hErrorCode = QueueEvent(MEError, GUID_NULL, hErrorCode, nullptr);
  }
}
void VideoRenderMediaStreamWinRT::SetMediaTypeAttributes(
  StreamDescription *pStreamDescription, IMFMediaType *pMediaType) {
  ThrowIfError(pMediaType->SetGUID(MF_MT_MAJOR_TYPE,
    pStreamDescription->guiMajorType));
  ThrowIfError(pMediaType->SetGUID(MF_MT_SUBTYPE,
    pStreamDescription->guiSubType));
  ThrowIfError(pMediaType->SetUINT32(MF_MT_DEFAULT_STRIDE,
    pStreamDescription->dwFrameWidth));
  ThrowIfError(MFSetAttributeRatio(pMediaType, MF_MT_FRAME_RATE,
    pStreamDescription->dwFrameRateNumerator,
    pStreamDescription->dwFrameRateDenominator));
  ThrowIfError(MFSetAttributeSize(pMediaType, MF_MT_FRAME_SIZE,
    pStreamDescription->dwFrameWidth,
    pStreamDescription->dwFrameHeight));
  ThrowIfError(pMediaType->SetUINT32(MF_MT_INTERLACE_MODE,
    MFVideoInterlace_Progressive));
  ThrowIfError(pMediaType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE));
  ThrowIfError(MFSetAttributeRatio(pMediaType, MF_MT_PIXEL_ASPECT_RATIO, 1,
    1));
}

void VideoRenderMediaStreamWinRT::SetSampleAttributes(
  SampleHeader *pSampleHeader, IMFSample *pSample) {
  ThrowIfError(pSample->SetSampleTime(pSampleHeader->ullTimestamp));

  ThrowIfError(pSample->SetUINT32(MFSampleExtension_CleanPoint, TRUE));
}

bool VideoRenderMediaStreamWinRT::ShouldDropSample(IMFSample *pSample) {
  bool fCleanPoint = MFGetAttributeUINT32(pSample,
     MFSampleExtension_CleanPoint, 0) > 0;
  bool fDrop = _flRate != 1.0f && !fCleanPoint;

  LONGLONG hnsTimeStamp = 0;
  ThrowIfError(pSample->GetSampleTime(&hnsTimeStamp));

  if (!fDrop && _fDropTime) {
    if (_fInitDropTime) {
      _hnsStartDroppingAt = hnsTimeStamp;
      _fInitDropTime = false;
    }

    fDrop = hnsTimeStamp < (_hnsStartDroppingAt + _hnsAmountToDrop);
    if (!fDrop) {
      LOG(LS_INFO) << "Ending dropping time on sample ts=" << hnsTimeStamp;
      ResetDropTime();
    } else {
      LOG(LS_INFO) << "Dropping sample ts=" << hnsTimeStamp;
    }
  }

  if (!fDrop && (_eDropMode == MF_DROP_MODE_1 || _fWaitingForCleanPoint)) {
    // Only key frames
    fDrop = !fCleanPoint;
    if (fCleanPoint) {
      _fWaitingForCleanPoint = false;
    }

    if (fDrop) {
      LOG(LS_INFO) << "Dropping sample ts=" << hnsTimeStamp;
    }
  }

  return fDrop;
}

void VideoRenderMediaStreamWinRT::CleanSampleQueue() {
  ComPtr<IUnknown> spEntry;
  ComPtr<IMFSample> spSample;

  // For video streams leave first key frame.
  std::deque<ComPtr<IUnknown> >::iterator iter = _samples.begin();
  while (iter != _samples.end()) {
    IUnknown **entry = spEntry.ReleaseAndGetAddressOf();
    *entry = iter->Get();
    if (SUCCEEDED(spEntry.As(&spSample)) &&
      MFGetAttributeUINT32(spSample.Get(), MFSampleExtension_CleanPoint, 0)) {
      break;
    }
  }

  while (!_samples.empty()) {
    _samples.pop_front();
  }

  if (spSample != nullptr) {
    _samples.push_back(spSample);
  }
}

void VideoRenderMediaStreamWinRT::ResetDropTime() {
  _fDropTime = false;
  _fInitDropTime = false;
  _hnsStartDroppingAt = 0;
  _hnsAmountToDrop = 0;
  _fWaitingForCleanPoint = true;
}

VideoRenderSourceOperation::VideoRenderSourceOperation(
  VideoRenderSourceOperation::Type opType)
  : _cRef(1),
    _opType(opType) {
  ZeroMemory(&_data, sizeof(_data));
}

VideoRenderSourceOperation::~VideoRenderSourceOperation() {
  PropVariantClear(&_data);
}

// IUnknown methods
IFACEMETHODIMP VideoRenderSourceOperation::QueryInterface(REFIID riid,
  void **ppv) {
  return E_NOINTERFACE;
}

IFACEMETHODIMP_(ULONG) VideoRenderSourceOperation::AddRef() {
  return InterlockedIncrement(&_cRef);
}

IFACEMETHODIMP_(ULONG) VideoRenderSourceOperation::Release() {
  long cRef = InterlockedDecrement(&_cRef);
  if (cRef == 0) {
    delete this;
  }
  return cRef;
}

HRESULT VideoRenderSourceOperation::SetData(const PROPVARIANT &varData) {
  return PropVariantCopy(&_data, &varData);
}

VideoRenderSourceStartOperation::VideoRenderSourceStartOperation(
  IMFPresentationDescriptor *pPD)
  : VideoRenderSourceOperation(VideoRenderSourceOperation::Operation_Start),
    _spPD(pPD) {
}

VideoRenderSourceStartOperation::~VideoRenderSourceStartOperation() {
}

VideoRenderSourceSetRateOperation::VideoRenderSourceSetRateOperation(
  BOOL fThin, float flRate)
  : VideoRenderSourceOperation(VideoRenderSourceOperation::Operation_SetRate),
    _fThin(fThin),
    _flRate(flRate) {
}

VideoRenderSourceSetRateOperation::~VideoRenderSourceSetRateOperation() {
}

//-------------------------------------------------------------------
// Place an operation on the queue.
// Public method.
//-------------------------------------------------------------------
template <class T, class TOperation>
HRESULT OpQueue<T, TOperation>::QueueOperation(TOperation *pOp) {
  HRESULT hr = S_OK;

  CriticalSectionScoped cs(m_critSec);

  m_OpQueue.push(pOp);

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
  if (m_OpQueue.size() > 0) {
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
  TOperation *pOp = nullptr;

  CriticalSectionScoped cs(m_critSec);

  if (m_OpQueue.size() > 0) {
    pOp = m_OpQueue.front();

    hr = ValidateOperation(pOp);
    if (SUCCEEDED(hr)) {
      m_OpQueue.pop();
    }
    if (SUCCEEDED(hr)) {
      (void)DispatchOperation(pOp);
    }
  }

  if (pOp != nullptr) {
    pOp->Release();
  }

  return hr;
}

VideoRenderMediaSourceWinRT::VideoRenderMediaSourceWinRT(void)
  : _critSec(CriticalSectionWrapper::CreateCriticalSection()),
    _cRef(1),
    _eSourceState(SourceState_Invalid),
    _flRate(1.0f),
    _bRenderTimeOffsetSet(false),
    _msRenderTimeOffset(0) {
  OpQueue<VideoRenderMediaSourceWinRT, VideoRenderSourceOperation>::m_critSec =
    _critSec;
}

VideoRenderMediaSourceWinRT::~VideoRenderMediaSourceWinRT(void) {
  if (_critSec) {
    delete _critSec;
  }
}

HRESULT VideoRenderMediaSourceWinRT::CreateInstance(
  VideoRenderMediaSourceWinRT **ppMediaSource) {
  HRESULT hr = S_OK;

  try {
    if (ppMediaSource == nullptr) {
      Throw(E_INVALIDARG);
    }

    ComPtr<VideoRenderMediaSourceWinRT> spSource;

    // Prepare the MF extension
    ThrowIfError(Microsoft::WRL::MakeAndInitialize<
      VideoRenderMediaSourceWinRT>(&spSource));

    spSource->Initialize();

    *ppMediaSource = spSource.Detach();
  } catch (Platform::Exception ^exc) {
    hr = exc->HResult;
  }

  if (!SUCCEEDED(hr)) {
    LOG_F(LS_ERROR) << "Render media source error: " << hr;
  }

  return hr;
}

// IMFMediaEventGenerator methods.
// Note: These methods call through to the event queue helper object.
IFACEMETHODIMP VideoRenderMediaSourceWinRT::BeginGetEvent(
  IMFAsyncCallback *pCallback, IUnknown *punkState) {
  HRESULT hr = S_OK;

  CriticalSectionScoped cs(_critSec);

  if (_eSourceState == SourceState_Shutdown) {
    hr = MF_E_SHUTDOWN;
  }

  if (SUCCEEDED(hr)) {
    hr = _spEventQueue->BeginGetEvent(pCallback, punkState);
  }

  if (!SUCCEEDED(hr)) {
    LOG_F(LS_ERROR) << "Render media source error: " << hr;
  }

  return hr;
}

IFACEMETHODIMP VideoRenderMediaSourceWinRT::EndGetEvent(
  IMFAsyncResult *pResult, IMFMediaEvent **ppEvent) {
  HRESULT hr = S_OK;

  CriticalSectionScoped cs(_critSec);

  if (_eSourceState == SourceState_Shutdown) {
    hr = MF_E_SHUTDOWN;
  }

  if (SUCCEEDED(hr)) {
    hr = _spEventQueue->EndGetEvent(pResult, ppEvent);
  }

  if (!SUCCEEDED(hr)) {
    LOG_F(LS_ERROR) << "Render media source error: " << hr;
  }

  return hr;
}

IFACEMETHODIMP VideoRenderMediaSourceWinRT::GetEvent(DWORD dwFlags,
  IMFMediaEvent **ppEvent) {
  // NOTE:
  // GetEvent can block indefinitely, so we don't hold the lock.
  // This requires some juggling with the event queue pointer.

  HRESULT hr = S_OK;

  ComPtr<IMFMediaEventQueue> spQueue;

  {
    CriticalSectionScoped cs(_critSec);

    // Check shutdown
    if (_eSourceState == SourceState_Shutdown) {
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
    LOG_F(LS_ERROR) << "Render media source error: " << hr;
  }

  return hr;
}

HRESULT VideoRenderMediaSourceWinRT::jSStart() {
  _eSourceState = SourceState_Started;

  VideoRenderMediaStreamWinRT *pStream =
    static_cast<VideoRenderMediaStreamWinRT*>(_spStream.Get());

  pStream->Start();
  pStream->SetActive(true);
  return S_OK;
}

VideoRenderMediaStreamWinRT*
VideoRenderMediaSourceWinRT::getCurrentActiveStream() {
  VideoRenderMediaStreamWinRT *pStream =
    static_cast<VideoRenderMediaStreamWinRT*>(_spStream.Get());

  return pStream;
}

HRESULT VideoRenderMediaSourceWinRT::requestSample(IUnknown* ptoken) {
  VideoRenderMediaStreamWinRT *pStream =
    static_cast<VideoRenderMediaStreamWinRT*>(_spStream.Get());

  return pStream->RequestSample(ptoken);
}

IFACEMETHODIMP VideoRenderMediaSourceWinRT::QueueEvent(MediaEventType met,
  REFGUID guidExtendedType, HRESULT hrStatus, PROPVARIANT const *pvValue) {
  HRESULT hr = S_OK;

  CriticalSectionScoped cs(_critSec);

  if (_eSourceState == SourceState_Shutdown) {
    hr = MF_E_SHUTDOWN;
  }

  if (SUCCEEDED(hr)) {
    hr = _spEventQueue->QueueEventParamVar(met, guidExtendedType, hrStatus,
      pvValue);
  }

  return hr;
}

// IMFMediaSource methods
IFACEMETHODIMP VideoRenderMediaSourceWinRT::CreatePresentationDescriptor(
    IMFPresentationDescriptor **ppPresentationDescriptor) {
  if (ppPresentationDescriptor == NULL) {
    return E_POINTER;
  }

  CriticalSectionScoped cs(_critSec);

  HRESULT hr = S_OK;
  if (_eSourceState == SourceState_Shutdown) {
    hr = MF_E_SHUTDOWN;
  }

  if (SUCCEEDED(hr) &&
    (_eSourceState == SourceState_Invalid || !_spPresentationDescriptor)) {
    hr = MF_E_NOT_INITIALIZED;
  }

  if (SUCCEEDED(hr)) {
    hr = _spPresentationDescriptor->Clone(ppPresentationDescriptor);
  }

  if (!SUCCEEDED(hr)) {
    LOG_F(LS_ERROR) << "Render media source error: " << hr;
  }

  return hr;
}

IFACEMETHODIMP VideoRenderMediaSourceWinRT::GetCharacteristics(
    DWORD *pdwCharacteristics) {
  if (pdwCharacteristics == NULL) {
    return E_POINTER;
  }

  CriticalSectionScoped cs(_critSec);

  HRESULT hr = S_OK;
  if (_eSourceState == SourceState_Shutdown) {
    hr = MF_E_SHUTDOWN;
  }

  if (SUCCEEDED(hr)) {
    *pdwCharacteristics = MFMEDIASOURCE_IS_LIVE;
  } else {
    LOG_F(LS_ERROR) << "Render media source error: " << hr;
  }

  return hr;
}

IFACEMETHODIMP VideoRenderMediaSourceWinRT::Pause() {
  return MF_E_INVALID_STATE_TRANSITION;
}

IFACEMETHODIMP VideoRenderMediaSourceWinRT::Shutdown() {
  CriticalSectionScoped cs(_critSec);

  HRESULT hr = S_OK;
  if (_eSourceState == SourceState_Shutdown) {
    hr = MF_E_SHUTDOWN;
  }

  if (SUCCEEDED(hr)) {
    if (_spEventQueue != nullptr) {
      _spEventQueue->Shutdown();
    }

    static_cast<VideoRenderMediaStreamWinRT*>(_spStream.Get())->Shutdown();

    _eSourceState = SourceState_Shutdown;

    _spStream.Reset();

    _spEventQueue.Reset();
  }

  if (!SUCCEEDED(hr)) {
    LOG_F(LS_ERROR) << "Render media source error: " << hr;
  }

  return hr;
}

IFACEMETHODIMP VideoRenderMediaSourceWinRT::Start(
    IMFPresentationDescriptor *pPresentationDescriptor,
    const GUID *pguidTimeFormat,
    const PROPVARIANT *pvarStartPos) {
  HRESULT hr = S_OK;

  // Check parameters.

  // Start position and presentation descriptor cannot be NULL.
  if (pvarStartPos == nullptr || pPresentationDescriptor == nullptr) {
    return E_INVALIDARG;
  }

  // Check the time format.
  if ((pguidTimeFormat != nullptr) && (*pguidTimeFormat != GUID_NULL)) {
    // Unrecognized time format GUID.
    return MF_E_UNSUPPORTED_TIME_FORMAT;
  }

  // Check the data type of the start position.
  if (pvarStartPos->vt != VT_EMPTY && pvarStartPos->vt != VT_I8) {
    return MF_E_UNSUPPORTED_TIME_FORMAT;
  }

  CriticalSectionScoped cs(_critSec);
  ComPtr<VideoRenderSourceStartOperation> spStartOp;

  if (_eSourceState != SourceState_Stopped &&
    _eSourceState != SourceState_Started) {
    hr = MF_E_INVALIDREQUEST;
  }

  if (SUCCEEDED(hr)) {
    // Check if the presentation description is valid.
    hr = ValidatePresentationDescriptor(pPresentationDescriptor);
  }

  if (SUCCEEDED(hr)) {
    // Prepare asynchronous operation attributes
    spStartOp.Attach(new (std::nothrow) VideoRenderSourceStartOperation(
      pPresentationDescriptor));
    if (!spStartOp) {
      hr = E_OUTOFMEMORY;
    }

    if (SUCCEEDED(hr)) {
      hr = spStartOp->SetData(*pvarStartPos);
    }
  }

  if (SUCCEEDED(hr)) {
    // Queue asynchronous operation
    hr = QueueOperation(spStartOp.Detach());
  }

  if (!SUCCEEDED(hr)) {
    LOG_F(LS_ERROR) << "Render media source error: " << hr;
  }

  return hr;
}

IFACEMETHODIMP VideoRenderMediaSourceWinRT::Stop() {
  HRESULT hr = S_OK;
  ComPtr<VideoRenderSourceOperation> spStopOp;
  spStopOp.Attach(new (std::nothrow) VideoRenderSourceOperation(
    VideoRenderSourceOperation::Operation_Stop));
  if (!spStopOp) {
    hr = E_OUTOFMEMORY;
  }

  if (SUCCEEDED(hr)) {
    // Queue asynchronous stop
    hr = QueueOperation(spStopOp.Detach());
  }

  if (!SUCCEEDED(hr)) {
    LOG_F(LS_ERROR) << "Render media source error: " << hr;
  }

  return hr;
}

IFACEMETHODIMP VideoRenderMediaSourceWinRT::GetService(
    _In_ REFGUID guidService,
    _In_ REFIID riid,
    _Out_opt_ LPVOID *ppvObject) {
  HRESULT hr = MF_E_UNSUPPORTED_SERVICE;

  if (ppvObject == nullptr) {
    return E_POINTER;
  }

  if (guidService == MF_RATE_CONTROL_SERVICE) {
    hr = QueryInterface(riid, ppvObject);
  } else if (guidService == MF_MEDIASOURCE_SERVICE) {
    hr = QueryInterface(riid, ppvObject);
  }

  return hr;
}

IFACEMETHODIMP VideoRenderMediaSourceWinRT::SetRate(BOOL fThin, float flRate) {
  if (fThin) {
    return MF_E_THINNING_UNSUPPORTED;
  }
  if (!IsRateSupported(flRate, &flRate)) {
    return MF_E_UNSUPPORTED_RATE;
  }

  CriticalSectionScoped cs(_critSec);
  HRESULT hr = S_OK;

  if (flRate == _flRate) {
    return S_OK;
  }

  ComPtr<VideoRenderSourceOperation> spSetRateOp;
  spSetRateOp.Attach(new (std::nothrow) VideoRenderSourceSetRateOperation(
    fThin, flRate));
  if (!spSetRateOp) {
    hr = E_OUTOFMEMORY;
  }

  if (SUCCEEDED(hr)) {
    // Queue asynchronous stop
    hr = QueueOperation(spSetRateOp.Detach());
  }

  if (!SUCCEEDED(hr)) {
    LOG_F(LS_ERROR) << "Render media source error: " << hr;
  }

  return hr;
}

IFACEMETHODIMP VideoRenderMediaSourceWinRT::GetRate(
    _Inout_opt_ BOOL *pfThin,
    _Inout_opt_ float *pflRate) {
  CriticalSectionScoped cs(_critSec);
  if (pfThin == nullptr || pflRate == nullptr) {
    return E_INVALIDARG;
  }

  *pfThin = FALSE;
  *pflRate = _flRate;

  return S_OK;
}

__override HRESULT VideoRenderMediaSourceWinRT::DispatchOperation(
  _In_ VideoRenderSourceOperation *pOp) {
  CriticalSectionScoped cs(_critSec);

  HRESULT hr = S_OK;
  if (_eSourceState == SourceState_Shutdown) {
    hr = MF_E_SHUTDOWN;
  }

  if (SUCCEEDED(hr)) {
    switch (pOp->GetOperationType()) {
    case VideoRenderSourceOperation::Operation_Start:
      DoStart(static_cast<VideoRenderSourceStartOperation *>(pOp));
      break;
    case VideoRenderSourceOperation::Operation_Stop:
      DoStop(pOp);
      break;
    case VideoRenderSourceOperation::Operation_SetRate:
      DoSetRate(static_cast<VideoRenderSourceSetRateOperation *>(pOp));
      break;
    default:
      hr = E_UNEXPECTED;
      break;
    }
  }

  if (!SUCCEEDED(hr)) {
    LOG_F(LS_ERROR) << "Render media source error: " << hr;
  }

  return hr;
}

__override HRESULT VideoRenderMediaSourceWinRT::ValidateOperation(
  _In_ VideoRenderSourceOperation *pOp) {
  return S_OK;
}

void VideoRenderMediaSourceWinRT::ProcessVideoFrame(
  const VideoFrame& videoFrame) {
  if (_eSourceState == SourceState_Started) {
    // Convert packet to MF sample
    ComPtr<VideoRenderMediaStreamWinRT> spStream;
    ComPtr<IMFSample> spSample;
    ComPtr<IMFMediaBuffer> spMediaBuffer;

    ThrowIfError(GetStreamById(1, &spStream));

    if (spStream->IsActive()) {
      HRESULT hr = MFCreateSample(&spSample);

      if (SUCCEEDED(hr)) {
        DWORD cbMaxLength = videoFrame.width() * videoFrame.height() * 3 / 2;
        hr = MFCreateMemoryBuffer(cbMaxLength, &spMediaBuffer);
      }

      BYTE* pbBuffer = NULL;
      if (SUCCEEDED(hr)) {
        DWORD cbMaxLength;
        DWORD cbCurrentLength;
        hr = spMediaBuffer->Lock(&pbBuffer, &cbMaxLength, &cbCurrentLength);
      }

      if (SUCCEEDED(hr)) {
        uint8_t* yBuffer = const_cast<uint8_t*>(videoFrame.buffer(kYPlane));
        for (int i = 0; i < videoFrame.height(); i++) {
          std::memcpy(pbBuffer, yBuffer, videoFrame.width());
          pbBuffer += videoFrame.width();
          yBuffer += videoFrame.stride(kYPlane);
        }
#if (WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP)
        // I420 to NV12 conversion
        uint8_t* uBuffer = const_cast<uint8_t*>(videoFrame.buffer(kUPlane));
        uint8_t* vBuffer = const_cast<uint8_t*>(videoFrame.buffer(kVPlane));
        for (int i = 0; i < videoFrame.height() / 2; i++) {
          for (int j = 0; j < videoFrame.width() / 2; j++) {
            pbBuffer[2*j+0] = uBuffer[j];
            pbBuffer[2*j+1] = vBuffer[j];
          }
          pbBuffer += videoFrame.width();
          uBuffer += videoFrame.stride(kUPlane);
          vBuffer += videoFrame.stride(kVPlane);
        }
#else
        uint8_t* uBuffer = const_cast<uint8_t*>(videoFrame.buffer(kUPlane));
        for (int i = 0; i < videoFrame.height() / 2; i++) {
          std::memcpy(pbBuffer, uBuffer, videoFrame.width() / 2);
          pbBuffer += videoFrame.width() / 2;
          uBuffer += videoFrame.stride(kUPlane);
        }
        uint8_t* vBuffer = const_cast<uint8_t*>(videoFrame.buffer(kVPlane));
        for (int i = 0; i < videoFrame.height() / 2; i++) {
          std::memcpy(pbBuffer, vBuffer, videoFrame.width() / 2);
          pbBuffer += videoFrame.width() / 2;
          vBuffer += videoFrame.stride(kVPlane);
        }
#endif
        hr  = spMediaBuffer->SetCurrentLength(videoFrame.width() *
          videoFrame.height() * 3 / 2);
      }

      if (SUCCEEDED(hr)) {
        spMediaBuffer->Unlock();
      }

      if (SUCCEEDED(hr)) {
        hr = spSample->AddBuffer(spMediaBuffer.Get());
      }

      if (SUCCEEDED(hr)) {
        // Forward sample to a proper stream.
        SampleHeader sampleHead;
        sampleHead.dwStreamId = 1;
        if (!_bRenderTimeOffsetSet) {
          _msRenderTimeOffset = videoFrame.render_time_ms();
          _bRenderTimeOffsetSet = true;
        }
        // conversion from millisecond to 100-nanosecond units
        sampleHead.ullTimestamp = (videoFrame.render_time_ms() -
          _msRenderTimeOffset) * 10000;
        spStream->ProcessSample(&sampleHead, spSample.Get());
      } else {
      }

      if (!SUCCEEDED(hr)) {
        LOG_F(LS_ERROR) << "Render media source error: " << hr;
      }
    }
  } else {
    // Throw(MF_E_UNEXPECTED);
  }
}

void VideoRenderMediaSourceWinRT::FrameSizeChange(int width, int height) {
  if (_eSourceState == SourceState_Stopped) {
    // Convert packet to MF sample
    ComPtr<VideoRenderMediaStreamWinRT> spStream;

    ThrowIfError(GetStreamById(1, &spStream));

    StreamDescription description;
    description.guiMajorType = MFMediaType_Video;
#if (WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP)
    description.guiSubType = MFVideoFormat_NV12;
#else
    description.guiSubType = MFVideoFormat_I420;
#endif
    description.dwFrameWidth = width;
    description.dwFrameHeight = height;
    description.dwFrameRateNumerator = 30;
    description.dwFrameRateDenominator = 1;
    description.dwStreamId = 1;

    spStream->ProcessFormatChange(&description);

    InitPresentationDescription();
  }
}

_Acquires_lock_(_critSec)
HRESULT VideoRenderMediaSourceWinRT::Lock() {
  _critSec->Enter();
  return S_OK;
}

_Releases_lock_(_critSec)
HRESULT VideoRenderMediaSourceWinRT::Unlock() {
  _critSec->Leave();
  return S_OK;
}

void VideoRenderMediaSourceWinRT::Initialize() {
  try {
    // Create the event queue helper.
    ThrowIfError(MFCreateEventQueue(&_spEventQueue));

    ComPtr<VideoRenderMediaStreamWinRT> spStream;

    // Dummy stream description. Stream format will be set by first incoming
    // sample.
    StreamDescription description;
    description.guiMajorType = MFMediaType_Video;
#if (WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP)
    description.guiSubType = MFVideoFormat_NV12;
#else
    description.guiSubType = MFVideoFormat_I420;
#endif
    // dummy initial values - chaned when first frame arrives
    description.dwFrameWidth = 320;
    description.dwFrameHeight = 240;
    description.dwFrameRateNumerator = 30;
    description.dwFrameRateDenominator = 1;
    description.dwStreamId = 1;

    ThrowIfError(VideoRenderMediaStreamWinRT::CreateInstance(
      &description, this, &spStream));

    _spStream = spStream.Detach();

    InitPresentationDescription();

    _eSourceState = SourceState_Stopped;
  } catch (Platform::Exception^) {
    Shutdown();
    throw;
  }
}

// Handle errors
void VideoRenderMediaSourceWinRT::HandleError(HRESULT hResult) {
  if (_eSourceState != SourceState_Shutdown) {
    // If we received an error at any other time (except shutdown)
    // send MEError event.
    QueueEvent(MEError, GUID_NULL, hResult, nullptr);
  }
}

// Returns stream object associated with given identifier
HRESULT VideoRenderMediaSourceWinRT::GetStreamById(DWORD dwId,
  VideoRenderMediaStreamWinRT **ppStream) {
  assert(ppStream);
  HRESULT hr = MF_E_NOT_FOUND;

  VideoRenderMediaStreamWinRT *pStream =
    static_cast<VideoRenderMediaStreamWinRT*>(_spStream.Get());
  if (pStream->GetId() == dwId) {
    *ppStream = pStream;
    (*ppStream)->AddRef();
    hr = S_OK;
  }

  if (!SUCCEEDED(hr)) {
    LOG_F(LS_ERROR) << "Render media source error: " << hr;
  }

  return hr;
}

void VideoRenderMediaSourceWinRT::InitPresentationDescription() {
  ComPtr<IMFPresentationDescriptor> spPresentationDescriptor;

  IMFStreamDescriptor *pStreamDescriptor = nullptr;

  ThrowIfError(_spStream->GetStreamDescriptor(&pStreamDescriptor));

  ThrowIfError(MFCreatePresentationDescriptor(1, &pStreamDescriptor,
    &spPresentationDescriptor));

  ThrowIfError(spPresentationDescriptor->SelectStream(0));

  _spPresentationDescriptor = spPresentationDescriptor;

  pStreamDescriptor->Release();
}

HRESULT VideoRenderMediaSourceWinRT::ValidatePresentationDescriptor(
  IMFPresentationDescriptor *pPD) {
  HRESULT hr = S_OK;
  BOOL fSelected = FALSE;
  DWORD cStreams = 0;

  if (_spStream == nullptr) {
    return E_UNEXPECTED;
  }

  // The caller's PD must have the same number of streams as ours.
  hr = pPD->GetStreamDescriptorCount(&cStreams);

  if (SUCCEEDED(hr)) {
    if (cStreams != 1) {
      hr = E_INVALIDARG;
    }
  }

  // The caller must select at least one stream.
  if (SUCCEEDED(hr)) {
    ComPtr<IMFStreamDescriptor> spSD;
    hr = pPD->GetStreamDescriptorByIndex(0, &fSelected, &spSD);
  }

  if (!SUCCEEDED(hr)) {
    LOG_F(LS_ERROR) << "Render media source error: " << hr;
  }

  return hr;
}

void VideoRenderMediaSourceWinRT::SelectStream(
  IMFPresentationDescriptor *pPD) {
  ComPtr<IMFStreamDescriptor> spSD;
  ComPtr<VideoRenderMediaStreamWinRT> spStream;
  DWORD nStreamId;
  BOOL fSelected;

  // Get next stream descriptor
  ThrowIfError(pPD->GetStreamDescriptorByIndex(0, &fSelected, &spSD));

  // Get stream id
  ThrowIfError(spSD->GetStreamIdentifier(&nStreamId));

  // Get simple net media stream
  ThrowIfError(GetStreamById(nStreamId, &spStream));

  // Remember if stream was selected
  bool fWasSelected = spStream->IsActive();
  ThrowIfError(spStream->SetActive(!!fSelected));

  if (fSelected) {
    // Choose event type to send
    MediaEventType met = (fWasSelected) ? MEUpdatedStream : MENewStream;
    ComPtr<IUnknown> spStreamUnk;

    spStream.As(&spStreamUnk);

    ThrowIfError(_spEventQueue->QueueEventParamUnk(met, GUID_NULL, S_OK,
      spStreamUnk.Get()));

    // Start the stream. The stream will send the appropriate event.
    ThrowIfError(spStream->Start());
  }
}

void VideoRenderMediaSourceWinRT::DoStart(
  VideoRenderSourceStartOperation *pOp) {
  assert(pOp->GetOperationType() ==
    VideoRenderSourceOperation::Operation_Start);
  ComPtr<IMFPresentationDescriptor> spPD = pOp->GetPresentationDescriptor();

  try {
    SelectStream(spPD.Get());

    _eSourceState = SourceState_Starting;

    _eSourceState = SourceState_Started;

    ThrowIfError(_spEventQueue->QueueEventParamVar(MESourceStarted, GUID_NULL,
      S_OK, &pOp->GetData()));
  } catch (Platform::Exception ^exc) {
    _spEventQueue->QueueEventParamVar(MESourceStarted, GUID_NULL, exc->HResult,
      nullptr);
  }
}

void VideoRenderMediaSourceWinRT::DoStop(VideoRenderSourceOperation *pOp) {
  assert(pOp->GetOperationType() ==
    VideoRenderSourceOperation::Operation_Stop);

  HRESULT hr = S_OK;
  try {
    VideoRenderMediaStreamWinRT *pStream =
      static_cast<VideoRenderMediaStreamWinRT*>(_spStream.Get());
    if (pStream->IsActive()) {
      ThrowIfError(pStream->Flush());
      ThrowIfError(pStream->Stop());
    }
    _eSourceState = SourceState_Stopped;
    _bRenderTimeOffsetSet = false;
    _msRenderTimeOffset = 0;
  } catch (Platform::Exception ^exc) {
    hr = exc->HResult;
  }

  if (!SUCCEEDED(hr)) {
    LOG_F(LS_ERROR) << "Render media source error: " << hr;
  }

  // Send the "stopped" event. This might include a failure code.
  (void)_spEventQueue->QueueEventParamVar(MESourceStopped, GUID_NULL, hr,
    nullptr);
}

void VideoRenderMediaSourceWinRT::DoSetRate(
  VideoRenderSourceSetRateOperation *pOp) {
  assert(pOp->GetOperationType() ==
    VideoRenderSourceOperation::Operation_SetRate);

  HRESULT hr = S_OK;
  try {
    VideoRenderMediaStreamWinRT *pStream =
      static_cast<VideoRenderMediaStreamWinRT*>(_spStream.Get());

    if (pStream->IsActive()) {
      ThrowIfError(pStream->Flush());
      ThrowIfError(pStream->SetRate(pOp->GetRate()));
    }

    _flRate = pOp->GetRate();
  } catch (Platform::Exception ^exc) {
    hr = exc->HResult;
  }

  if (!SUCCEEDED(hr)) {
    LOG_F(LS_ERROR) << "Render media source error: " << hr;
  }

  // Send the "rate changed" event. This might include a failure code.
  (void)_spEventQueue->QueueEventParamVar(MESourceRateChanged, GUID_NULL, hr,
    nullptr);
}

bool VideoRenderMediaSourceWinRT::IsRateSupported(float flRate,
  float *pflAdjustedRate) {
  if (flRate < 0.00001f && flRate > -0.00001f) {
    *pflAdjustedRate = 0.0f;
    return true;
  } else if (flRate < 1.0001f && flRate > 0.9999f) {
    *pflAdjustedRate = 1.0f;
    return true;
  }
  return false;
}
}  // namespace webrtc
