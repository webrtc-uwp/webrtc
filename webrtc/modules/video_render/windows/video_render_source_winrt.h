/*
*  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
*
*  Use of this source code is governed by a BSD-style license
*  that can be found in the LICENSE file in the root of the source
*  tree. An additional intellectual property rights grant can be found
*  in the file PATENTS.  All contributing project authors may
*  be found in the AUTHORS file in the root of the source tree.
*/

#ifndef WEBRTC_MODULES_VIDEO_RENDER_WINDOWS_VIDEO_RENDER_SOURCE_WINRT_H_
#define WEBRTC_MODULES_VIDEO_RENDER_WINDOWS_VIDEO_RENDER_SOURCE_WINRT_H_

#include <wrl/implements.h>

#include <mfidl.h>

#include <windows.media.h>
#include <windows.media.core.h>

#include <queue>
#include <deque>

#include "webrtc/modules/video_render/video_render_defines.h"

namespace webrtc {
class CriticalSectionWrapper;
class VideoRenderMediaSourceWinRT;

enum SourceState {
  // Invalid state, source cannot be used
  SourceState_Invalid,
  // Streaming started
  SourceState_Starting,
  // Streaming started
  SourceState_Started,
  // Streanung stopped
  SourceState_Stopped,
  // Source is shut down
  SourceState_Shutdown,
};

struct StreamDescription {
  GUID guiMajorType;
  GUID guiSubType;
  DWORD dwStreamId;
  DWORD dwFrameWidth;
  DWORD dwFrameHeight;
  DWORD dwFrameRateNumerator;
  DWORD dwFrameRateDenominator;
};

struct SampleHeader {
  DWORD dwStreamId;
  LONGLONG ullTimestamp;
  LONGLONG ullDuration;
};

class VideoRenderMediaStreamWinRT :
    public IMFMediaStream,
    public IMFQualityAdvise2,
    public IMFGetService {
 public:
  static HRESULT CreateInstance(StreamDescription *pStreamDescription,
    VideoRenderMediaSourceWinRT *pSource,
    VideoRenderMediaStreamWinRT **ppStream);

  // IUnknown
  IFACEMETHOD(QueryInterface) (REFIID iid, void **ppv);
  IFACEMETHOD_(ULONG, AddRef) ();
  IFACEMETHOD_(ULONG, Release) ();

  // IMFMediaEventGenerator
  IFACEMETHOD(BeginGetEvent) (IMFAsyncCallback *pCallback,
    IUnknown *punkState);
  IFACEMETHOD(EndGetEvent) (IMFAsyncResult *pResult, IMFMediaEvent **ppEvent);
  IFACEMETHOD(GetEvent) (DWORD dwFlags, IMFMediaEvent **ppEvent);
  IFACEMETHOD(QueueEvent) (MediaEventType met, REFGUID guidExtendedType,
    HRESULT hrStatus, const PROPVARIANT *pvValue);

  // IMFMediaStream
  IFACEMETHOD(GetMediaSource) (IMFMediaSource **ppMediaSource);
  IFACEMETHOD(GetStreamDescriptor) (IMFStreamDescriptor **ppStreamDescriptor);
  IFACEMETHOD(RequestSample) (IUnknown *pToken);

  // IMFQualityAdvise
  IFACEMETHOD(SetDropMode) (_In_ MF_QUALITY_DROP_MODE eDropMode);
  IFACEMETHOD(SetQualityLevel) (_In_ MF_QUALITY_LEVEL eQualityLevel);
  IFACEMETHOD(GetDropMode) (_Out_ MF_QUALITY_DROP_MODE *peDropMode);
  IFACEMETHOD(GetQualityLevel)(_Out_ MF_QUALITY_LEVEL *peQualityLevel);
  IFACEMETHOD(DropTime) (_In_ LONGLONG hnsAmountToDrop);

  // IMFQualityAdvise2
  IFACEMETHOD(NotifyQualityEvent) (_In_opt_ IMFMediaEvent *pEvent,
    _Out_ DWORD *pdwFlags);

  // IMFGetService
  IFACEMETHOD(GetService) (_In_ REFGUID guidService,
    _In_ REFIID riid,
    _Out_ LPVOID *ppvObject);

  // Other public methods
  HRESULT Start();
  HRESULT Stop();
  HRESULT SetRate(float flRate);
  HRESULT Flush();
  HRESULT Shutdown();
  void ProcessSample(SampleHeader *pSampleHeader, IMFSample *pSample);
  void ProcessFormatChange(StreamDescription *pStreamDescription);
  const StreamDescription& getCurrentStreamDescription() const {
    return _currentStreamDescription; }
  HRESULT SetActive(bool fActive);
  bool IsActive() const { return _fActive; }
  SourceState GetState() const { return _eSourceState; }

  DWORD GetId() const { return _dwId; }

 protected:
  explicit VideoRenderMediaStreamWinRT(VideoRenderMediaSourceWinRT *pSource);
  ~VideoRenderMediaStreamWinRT(void);

 private:
  class SourceLock;

 private:
  void Initialize(StreamDescription *pStreamDescription);
  void DeliverSamples();
  void SetMediaTypeAttributes(StreamDescription *pStreamDescription,
    IMFMediaType *pMediaType);
  void SetSampleAttributes(SampleHeader *pSampleHeader, IMFSample *pSample);
  void HandleError(HRESULT hErrorCode);

  bool ShouldDropSample(IMFSample *pSample);
  void CleanSampleQueue();
  void ResetDropTime();

 private:
  ULONG _cRef;
  SourceState _eSourceState;
  Microsoft::WRL::ComPtr<VideoRenderMediaSourceWinRT>_spSource;
  Microsoft::WRL::ComPtr<IMFMediaEventQueue> _spEventQueue;
  Microsoft::WRL::ComPtr<IMFStreamDescriptor> _spStreamDescriptor;

  std::deque<Microsoft::WRL::ComPtr<IUnknown> > _samples;
  std::deque<Microsoft::WRL::ComPtr<IUnknown> > _tokens;

  DWORD _dwId;
  bool _fActive;
  float _flRate;

  MF_QUALITY_DROP_MODE _eDropMode;
  bool _fDiscontinuity;
  bool _fDropTime;
  bool _fInitDropTime;
  bool _fWaitingForCleanPoint;
  LONGLONG _hnsStartDroppingAt;
  LONGLONG _hnsAmountToDrop;
  StreamDescription _currentStreamDescription;
};

// Base class representing asyncronous source operation
class VideoRenderSourceOperation : public IUnknown {
 public:
  enum Type {
    // Start the source
    Operation_Start,
    // Stop the source
    Operation_Stop,
    // Set rate
    Operation_SetRate,
  };

 public:
  explicit VideoRenderSourceOperation(Type opType);
  virtual ~VideoRenderSourceOperation();
  // IUnknown
  IFACEMETHOD(QueryInterface) (REFIID riid, void **ppv);
  IFACEMETHOD_(ULONG, AddRef) ();
  IFACEMETHOD_(ULONG, Release) ();

  Type GetOperationType() const { return _opType; }
  const PROPVARIANT &GetData() const { return _data; }
  HRESULT SetData(const PROPVARIANT &varData);

 private:
  ULONG _cRef;
  Type _opType;
  PROPVARIANT _data;
};

// Start operation
class VideoRenderSourceStartOperation : public VideoRenderSourceOperation {
 public:
  explicit VideoRenderSourceStartOperation(IMFPresentationDescriptor *pPD);
  ~VideoRenderSourceStartOperation();

  IMFPresentationDescriptor *GetPresentationDescriptor() {
    return _spPD.Get(); }

 private:
  Microsoft::WRL::ComPtr<IMFPresentationDescriptor> _spPD;
};

// SetRate operation
class VideoRenderSourceSetRateOperation : public VideoRenderSourceOperation {
 public:
  VideoRenderSourceSetRateOperation(BOOL fThin, float flRate);
  ~VideoRenderSourceSetRateOperation();

  BOOL IsThin() const { return _fThin; }
  float GetRate() const { return _flRate; }

 private:
  BOOL _fThin;
  float _flRate;
};

template<class T>
class AsyncCallback : public IMFAsyncCallback {
 public:
  typedef HRESULT(T::*InvokeFn)(IMFAsyncResult *pAsyncResult);

  AsyncCallback(T *pParent, InvokeFn fn) : _pParent(pParent), _pInvokeFn(fn) {
  }

  // IUnknown
  STDMETHODIMP_(ULONG) AddRef() {
    // Delegate to parent class.
    return _pParent->AddRef();
  }
  STDMETHODIMP_(ULONG) Release() {
    // Delegate to parent class.
    return _pParent->Release();
  }
  STDMETHODIMP QueryInterface(REFIID iid, void** ppv) {
    if (!ppv) {
      return E_POINTER;
    }
    if (iid == __uuidof(IUnknown)) {
      *ppv = static_cast<IUnknown*>(static_cast<IMFAsyncCallback*>(this));
    } else if (iid == __uuidof(IMFAsyncCallback)) {
      *ppv = static_cast<IMFAsyncCallback*>(this);
    } else {
      *ppv = NULL;
      return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
  }

  // IMFAsyncCallback methods
  STDMETHODIMP GetParameters(DWORD*, DWORD*) {
    // Implementation of this method is optional.
    return E_NOTIMPL;
  }

  STDMETHODIMP Invoke(IMFAsyncResult* pAsyncResult) {
    return (_pParent->*_pInvokeFn)(pAsyncResult);
  }

  T *_pParent;
  InvokeFn _pInvokeFn;
};

template <class T, class TOperation>
class OpQueue {
 public:
  HRESULT QueueOperation(TOperation *pOp);

 protected:
  HRESULT ProcessQueue();
  HRESULT ProcessQueueAsync(IMFAsyncResult *pResult);

  virtual HRESULT DispatchOperation(TOperation *pOp) = 0;
  virtual HRESULT ValidateOperation(TOperation *pOp) = 0;

  OpQueue()
    : m_OnProcessQueue(static_cast<T *>(this), &OpQueue::ProcessQueueAsync),
      m_critSec(NULL) {
  }

  explicit OpQueue(CriticalSectionWrapper* critsec)
    : m_OnProcessQueue(static_cast<T *>(this), &OpQueue::ProcessQueueAsync),
      m_critSec(critsec) {
  }

  virtual ~OpQueue() {
  }

 protected:
  std::queue<TOperation*> m_OpQueue;
  CriticalSectionWrapper* m_critSec;
  AsyncCallback<T> m_OnProcessQueue;
};

class VideoRenderMediaSourceWinRT :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<
            Microsoft::WRL::RuntimeClassType::WinRtClassicComMix >,
        ABI::Windows::Media::Core::IMediaSource,
        IMFMediaSource,
        IMFGetService,
        IMFRateControl > ,
    public OpQueue<VideoRenderMediaSourceWinRT, VideoRenderSourceOperation> {
  InspectableClass(L"webrtc::VideoRenderMediaSourceWinRT", BaseTrust)

 public:
  static HRESULT CreateInstance(VideoRenderMediaSourceWinRT **ppMediaSource);

  // IMFMediaEventGenerator
  IFACEMETHOD(BeginGetEvent) (IMFAsyncCallback *pCallback,
    IUnknown *punkState);
  IFACEMETHOD(EndGetEvent) (IMFAsyncResult *pResult, IMFMediaEvent **ppEvent);
  IFACEMETHOD(GetEvent) (DWORD dwFlags, IMFMediaEvent **ppEvent);
  IFACEMETHOD(QueueEvent) (MediaEventType met, REFGUID guidExtendedType,
    HRESULT hrStatus, const PROPVARIANT *pvValue);

  // IMFMediaSource
  IFACEMETHOD(CreatePresentationDescriptor) (
      IMFPresentationDescriptor **ppPresentationDescriptor);
  IFACEMETHOD(GetCharacteristics) (DWORD *pdwCharacteristics);
  IFACEMETHOD(Pause) ();
  IFACEMETHOD(Shutdown) ();
  IFACEMETHOD(Start) (
      IMFPresentationDescriptor *pPresentationDescriptor,
      const GUID *pguidTimeFormat,
      const PROPVARIANT *pvarStartPosition);
  IFACEMETHOD(Stop)();

  // IMFGetService
  IFACEMETHOD(GetService) (
      _In_ REFGUID guidService,
      _In_ REFIID riid,
      _Out_opt_ LPVOID *ppvObject);

  // IMFRateControl
  IFACEMETHOD(SetRate) (BOOL fThin, float flRate);
  IFACEMETHOD(GetRate) (_Inout_opt_ BOOL *pfThin, _Inout_opt_ float *pflRate);

  // OpQueue
  __override HRESULT DispatchOperation(_In_ VideoRenderSourceOperation *pOp);
  __override HRESULT ValidateOperation(_In_ VideoRenderSourceOperation *pOp);

  void ProcessVideoFrame(const VideoFrame& videoFrame);
  void FrameSizeChange(int width, int height);
  // start source triggerred by WinJs
  HRESULT jSStart();
  HRESULT requestSample(IUnknown* ptoken);
  VideoRenderMediaStreamWinRT* getCurrentActiveStream();
  _Acquires_lock_(_critSec)
  HRESULT Lock();
  _Releases_lock_(_critSec)
  HRESULT Unlock();

 public:
  VideoRenderMediaSourceWinRT();
  ~VideoRenderMediaSourceWinRT();

 private:
  void Initialize();

  void HandleError(HRESULT hResult);
  HRESULT GetStreamById(DWORD dwId, VideoRenderMediaStreamWinRT **ppStream);
  void InitPresentationDescription();
  HRESULT ValidatePresentationDescriptor(IMFPresentationDescriptor *pPD);
  void SelectStream(IMFPresentationDescriptor *pPD);

  void DoStart(VideoRenderSourceStartOperation *pOp);
  void DoStop(VideoRenderSourceOperation *pOp);
  void DoSetRate(VideoRenderSourceSetRateOperation *pOp);

  bool IsRateSupported(float flRate, float *pflAdjustedRate);

 private:
  ULONG _cRef;
  CriticalSectionWrapper* _critSec;
  SourceState _eSourceState;

  Microsoft::WRL::ComPtr<IMFMediaEventQueue> _spEventQueue;
  Microsoft::WRL::ComPtr<IMFPresentationDescriptor> _spPresentationDescriptor;
  Microsoft::WRL::ComPtr<IMFMediaStream> _spStream;

  float _flRate;
  bool _bRenderTimeOffsetSet;
  int64_t _msRenderTimeOffset;
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_RENDER_WINDOWS_VIDEO_RENDER_SOURCE_WINRT_H_
