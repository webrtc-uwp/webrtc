/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#pragma warning(disable: 4995)  // name was marked as #pragma deprecated

#if (_MSC_VER >= 1310) && (_MSC_VER < 1400)
// Reports the major and minor versions of the compiler.
// For example, 1310 for Microsoft Visual C++ .NET 2003. 1310 represents
// version 13 and a 1.0 point release.
// The Visual C++ 2005 compiler version is 1400.
// Type cl /? at the command line to see the major and minor versions of your
// compiler along with the build number.
#pragma message(">> INFO: Windows Core Audio is not supported in VS 2003")
#endif

#include "webrtc/modules/audio_device/audio_device_config.h"

#ifdef WEBRTC_WINDOWS_CORE_AUDIO_BUILD

#include "webrtc/modules/audio_device/win/audio_device_wasapi_win.h"

#include <assert.h>
#include <string.h>

#include <windows.h>
#include <comdef.h>
#include <dmo.h>
#include <Functiondiscoverykeys_devpkey.h>
#include <mmsystem.h>
#include <endpointvolume.h>
#include <strsafe.h>
#include <uuids.h>
#include <ppltasks.h>
#include <collection.h>

#include "webrtc/system_wrappers/include/sleep.h"
#include "webrtc/system_wrappers/include/trace.h"
#include "webrtc/system_wrappers/include/logging.h"

#include "webrtc/base/win32.h"

using Windows::Foundation::EventRegistrationToken;
using Windows::Foundation::TypedEventHandler;

#define KSAUDIO_SPEAKER_MONO        (SPEAKER_FRONT_CENTER)
#define KSAUDIO_SPEAKER_STEREO      (SPEAKER_FRONT_LEFT | \
                                     SPEAKER_FRONT_RIGHT)
#define KSAUDIO_SPEAKER_QUAD        (SPEAKER_FRONT_LEFT  | \
                                     SPEAKER_FRONT_RIGHT | \
                                     SPEAKER_BACK_LEFT   | \
                                     SPEAKER_BACK_RIGHT)

#define KSAUDIO_SPEAKER_SURROUND    (SPEAKER_FRONT_LEFT   | \
                                     SPEAKER_FRONT_RIGHT  | \
                                     SPEAKER_FRONT_CENTER | \
                                     SPEAKER_BACK_CENTER)

#define KSAUDIO_SPEAKER_5POINT1     (SPEAKER_FRONT_LEFT    | \
                                     SPEAKER_FRONT_RIGHT   | \
                                     SPEAKER_FRONT_CENTER  | \
                                     SPEAKER_LOW_FREQUENCY | \
                                     SPEAKER_BACK_LEFT     | \
                                     SPEAKER_BACK_RIGHT)

#define KSAUDIO_SPEAKER_7POINT1     (SPEAKER_FRONT_LEFT           | \
                                     SPEAKER_FRONT_RIGHT          | \
                                     SPEAKER_FRONT_CENTER         | \
                                     SPEAKER_LOW_FREQUENCY        | \
                                     SPEAKER_BACK_LEFT            | \
                                     SPEAKER_BACK_RIGHT           | \
                                     SPEAKER_FRONT_LEFT_OF_CENTER | \
                                     SPEAKER_FRONT_RIGHT_OF_CENTER)

// These defines are not available for Windows Store applications.
// However, those flags are needed and acccepted by WASAPI in order to support
// multichannel devices
#define AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM      0x80000000
#define AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY 0x08000000

// Macro that calls a COM method returning HRESULT value.
#define EXIT_ON_ERROR(hres)    do { if (FAILED(hres)) goto Exit; } while (0)

// Macro that continues to a COM error.
#define CONTINUE_ON_ERROR(hres) do { if (FAILED(hres)) goto Next; } while (0)

// Macro that releases a COM object if not NULL.
#define SAFE_RELEASE(p) do { if ((p)) { (p)->Release(); (p) = NULL; } } \
  while (0)

#define ROUND(x) ((x) >=0 ? static_cast<int>((x) + 0.5) : \
  static_cast<int>((x) - 0.5))

// REFERENCE_TIME time units per millisecond
#define REFTIMES_PER_MILLISEC  10000

#define MAXERRORLENGTH 256

#pragma comment(lib, "Mmdevapi.lib")
#pragma comment(lib, "MFuuid.lib")
#pragma comment(lib, "MFReadWrite.lib")
#pragma comment(lib, "Mfplat.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "ole32.lib")

typedef struct tagTHREADNAME_INFO {
  DWORD dwType;        // must be 0x1000
  LPCSTR szName;       // pointer to name (in user addr space)
  DWORD dwThreadID;    // thread ID (-1=caller thread)
  DWORD dwFlags;       // reserved for future use, must be zero
} THREADNAME_INFO;

namespace webrtc {
namespace {

enum { COM_THREADING_MODEL = COINIT_MULTITHREADED };

enum {
  kAecCaptureStreamIndex = 0,
  kAecRenderStreamIndex = 1
};

}  // namespace

ref class DefaultAudioDeviceWatcher {
internal:
  DefaultAudioDeviceWatcher(AudioDeviceWindowsWasapi* observer);
public:
  virtual ~DefaultAudioDeviceWatcher();

private:
  void DefaultAudioCaptureDeviceChanged(Platform::Object^ sender,
    DefaultAudioCaptureDeviceChangedEventArgs^ args);
  void DefaultAudioRenderDeviceChanged(Platform::Object^ sender,
    DefaultAudioRenderDeviceChangedEventArgs^ args);
private:
  AudioDeviceWindowsWasapi* _observer;
  EventRegistrationToken _defaultCaptureChangedToken;
  EventRegistrationToken _defaultRenderChangedToken;
};

DefaultAudioDeviceWatcher::DefaultAudioDeviceWatcher(
  AudioDeviceWindowsWasapi* observer) : _observer(observer) {

  _defaultCaptureChangedToken = MediaDevice::DefaultAudioCaptureDeviceChanged::add(
    ref new TypedEventHandler<Platform::Object^,
                              DefaultAudioCaptureDeviceChangedEventArgs^>
      (this,&DefaultAudioDeviceWatcher::DefaultAudioCaptureDeviceChanged));
  _defaultRenderChangedToken = MediaDevice::DefaultAudioRenderDeviceChanged::add(
    ref new TypedEventHandler<Platform::Object^,
                              DefaultAudioRenderDeviceChangedEventArgs^>
      (this,&DefaultAudioDeviceWatcher::DefaultAudioRenderDeviceChanged));
}

DefaultAudioDeviceWatcher::~DefaultAudioDeviceWatcher() {
  MediaDevice::DefaultAudioRenderDeviceChanged::remove(
    _defaultCaptureChangedToken);
  MediaDevice::DefaultAudioRenderDeviceChanged::remove(
    _defaultRenderChangedToken);
}

void DefaultAudioDeviceWatcher::DefaultAudioCaptureDeviceChanged(
  Platform::Object^ sender,
  DefaultAudioCaptureDeviceChangedEventArgs^ args) {
  if (_observer != nullptr) {
    _observer->DefaultAudioCaptureDeviceChanged(args);
  }
}

void DefaultAudioDeviceWatcher::DefaultAudioRenderDeviceChanged(
  Platform::Object^ sender,
  DefaultAudioRenderDeviceChangedEventArgs^ args) {
  if (_observer != nullptr) {
    _observer->DefaultAudioRenderDeviceChanged(args);
  }
}

AudioDeviceWindowsWasapi* AudioInterfaceActivator::m_AudioDevice = nullptr;
AudioInterfaceActivator::ActivatorDeviceType
                                AudioInterfaceActivator::m_DeviceType = eNone;

HRESULT AudioInterfaceActivator::ActivateCompleted(
  IActivateAudioInterfaceAsyncOperation  *pAsyncOp) {
  HRESULT hr = S_OK;
  HRESULT hrActivateResult = S_OK;
  IUnknown *punkAudioInterface = nullptr;
  IAudioClient2 *audioClient = nullptr;
  WAVEFORMATEX *mixFormat = nullptr;

  if (m_DeviceType == eInputDevice) {

    // Check for a successful activation result
    hr = pAsyncOp->GetActivateResult(&hrActivateResult, &punkAudioInterface);
    if (SUCCEEDED(hr) && SUCCEEDED(hrActivateResult)) {
      // Get the pointer for the Audio Client
      hr = punkAudioInterface->QueryInterface(IID_PPV_ARGS(&audioClient));
      if ((E_POINTER == hr) || (E_NOINTERFACE == hr)) {
        hr = E_FAIL;
        goto exit;
      }

      AudioClientProperties prop = { 0 };
      prop.cbSize = sizeof(AudioClientProperties);
      prop.bIsOffload = 0;
      prop.eCategory = AudioCategory_Communications;
      prop.Options = AUDCLNT_STREAMOPTIONS_NONE;
      hr = audioClient->SetClientProperties(&prop);

      if (FAILED(hr)) {
        goto exit;
      }

      hr = audioClient->GetMixFormat(&mixFormat);
      if (FAILED(hr)) {
        goto exit;
      }

      if (SUCCEEDED(hr)) {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
          "Audio Engine's current capturing mix format:");
        // format type
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
          "wFormatTag     : 0x%X (%u)", mixFormat->wFormatTag,
          mixFormat->wFormatTag);
        // number of channels (i.e. mono, stereo...)
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
          "nChannels      : %d", mixFormat->nChannels);
        // sample rate
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
          "nSamplesPerSec : %d", mixFormat->nSamplesPerSec);
        // for buffer estimation
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
          "nAvgBytesPerSec: %d", mixFormat->nAvgBytesPerSec);
        // block size of data
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
          "nBlockAlign    : %d", mixFormat->nBlockAlign);
        // number of bits per sample of mono data
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
          "wBitsPerSample : %d", mixFormat->wBitsPerSample);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
          "cbSize         : %d", mixFormat->cbSize);
      }

      WAVEFORMATEX Wfx = WAVEFORMATEX();
      WAVEFORMATEX* pWfxClosestMatch = NULL;

      // Set wave format
      Wfx.wFormatTag = WAVE_FORMAT_PCM;
      Wfx.wBitsPerSample = 16;
      Wfx.cbSize = 0;

      const int freqs[6] = { 48000, 44100, 16000, 96000, 32000, 8000 };
      hr = S_FALSE;

      // Iterate over frequencies and channels, in order of priority
      for (int freq = 0; freq < sizeof(freqs) / sizeof(freqs[0]); freq++) {
        for (int chan = 0; chan < sizeof(m_AudioDevice->_recChannelsPrioList) /
          sizeof(m_AudioDevice->_recChannelsPrioList[0]); chan++) {
          Wfx.nChannels = m_AudioDevice->_recChannelsPrioList[chan];
          Wfx.nSamplesPerSec = freqs[freq];
          Wfx.nBlockAlign = Wfx.nChannels * Wfx.wBitsPerSample / 8;
          Wfx.nAvgBytesPerSec = Wfx.nSamplesPerSec * Wfx.nBlockAlign;
          // If the method succeeds and the audio endpoint device supports the
          // specified stream format, it returns S_OK. If the method succeeds
          // and provides a closest match to the specified format, it returns
          // S_FALSE.
          hr = audioClient->IsFormatSupported(
            AUDCLNT_SHAREMODE_SHARED,
            &Wfx,
            &pWfxClosestMatch);
          if (hr == S_OK) {
            break;
          } else {
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
              "nChannels=%d, nSamplesPerSec=%d is not supported",
              Wfx.nChannels, Wfx.nSamplesPerSec);
            // if number of channels is more than 2, keep mix format which is
            // prefered by the engine. Wasapi will handle channel mixing.
            if (mixFormat->nChannels > 2) {
              hr = S_OK;
              break;
            }
          }
        }
        if (hr == S_OK)
          break;
      }

      if (hr == S_OK) {
        m_AudioDevice->_recAudioFrameSize = Wfx.nBlockAlign;
        m_AudioDevice->_recSampleRate = Wfx.nSamplesPerSec;
        m_AudioDevice->_recBlockSize = Wfx.nSamplesPerSec / 100;
        m_AudioDevice->_recChannels = Wfx.nChannels;

        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
          "VoE selected this capturing format:");
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
          "wFormatTag        : 0x%X (%u)", Wfx.wFormatTag, Wfx.wFormatTag);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
          "nChannels         : %d", Wfx.nChannels);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
          "nSamplesPerSec    : %d", Wfx.nSamplesPerSec);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
          "nAvgBytesPerSec   : %d", Wfx.nAvgBytesPerSec);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
          "nBlockAlign       : %d", Wfx.nBlockAlign);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
          "wBitsPerSample    : %d", Wfx.wBitsPerSample);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
          "cbSize            : %d", Wfx.cbSize);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
          "Additional settings:");
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
          "_recAudioFrameSize: %d", m_AudioDevice->_recAudioFrameSize);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
          "_recBlockSize     : %d", m_AudioDevice->_recBlockSize);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
          "_recChannels      : %d", m_AudioDevice->_recChannels);
      }
      // Create a capturing stream.
      hr = audioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,                 // share Audio Engine with
                                                  // other applications
        AUDCLNT_STREAMFLAGS_EVENTCALLBACK |       // processing of the audio
                                                  // buffer by the client will
                                                  // be event driven
        AUDCLNT_STREAMFLAGS_NOPERSIST |           // volume and mute settings
                                                  // for an audio session will
                                                  // not persist across system
                                                  // restarts
        AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |      // support for multichannel
                                                  // devices
        AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,  // keep default quality
        0,                                        // required for event-driven
                                                  // shared mode
        0,                                        // periodicity
        &Wfx,                                     // selected wave format
        NULL);                                    // session GUID

      if (hr != S_OK) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, m_AudioDevice->_id,
          "IAudioClient::Initialize() failed:");
        if (pWfxClosestMatch != NULL) {
          WEBRTC_TRACE(kTraceError, kTraceAudioDevice, m_AudioDevice->_id,
            "closest mix format: #channels=%d, samples/sec=%d, bits/sample=%d",
            pWfxClosestMatch->nChannels, pWfxClosestMatch->nSamplesPerSec,
            pWfxClosestMatch->wBitsPerSample);
        } else {
          WEBRTC_TRACE(kTraceError, kTraceAudioDevice, m_AudioDevice->_id,
            "no format suggested");
        }
      }

      if (FAILED(hr)) {
        goto exit;
      }

      // Get the capture client
      hr = audioClient->GetService(__uuidof(IAudioCaptureClient),
        reinterpret_cast<void**>(&m_AudioDevice->_ptrCaptureClient));
      if (FAILED(hr)) {
        goto exit;
      }
      m_AudioDevice->_ptrClientIn = audioClient;
    }
  } else if (m_DeviceType == eOutputDevice) {
    // Check for a successful activation result
    hr = pAsyncOp->GetActivateResult(&hrActivateResult, &punkAudioInterface);
    if (SUCCEEDED(hr) && SUCCEEDED(hrActivateResult)) {
      // Get the pointer for the Audio Client
      hr = punkAudioInterface->QueryInterface(IID_PPV_ARGS(&audioClient));
      if ((E_POINTER == hr) || (E_NOINTERFACE == hr)) {
        hr = E_FAIL;
        goto exit;
      }

      AudioClientProperties prop = { 0 };
      prop.cbSize = sizeof(AudioClientProperties);
      prop.bIsOffload = 0;
      prop.eCategory = AudioCategory_Communications;
      prop.Options = AUDCLNT_STREAMOPTIONS_NONE;
      hr = audioClient->SetClientProperties(&prop);

      if (FAILED(hr)) {
        goto exit;
      }

      hr = audioClient->GetMixFormat(&mixFormat);
      if (FAILED(hr)) {
        goto exit;
      }

      if (SUCCEEDED(hr)) {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
          "Audio Engine's current rendering mix format:");
        // format type
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
          "wFormatTag     : 0x%X (%u)",
          mixFormat->wFormatTag, mixFormat->wFormatTag);
        // number of channels (i.e. mono, stereo...)
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
          "nChannels      : %d", mixFormat->nChannels);
        // sample rate
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
          "nSamplesPerSec : %d", mixFormat->nSamplesPerSec);
        // for buffer estimation
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
          "nAvgBytesPerSec: %d", mixFormat->nAvgBytesPerSec);
        // block size of data
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
          "nBlockAlign    : %d", mixFormat->nBlockAlign);
        // number of bits per sample of mono data
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
          "wBitsPerSample : %d", mixFormat->wBitsPerSample);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
          "cbSize         : %d", mixFormat->cbSize);
      }

      WAVEFORMATEX Wfx = WAVEFORMATEX();
      WAVEFORMATEX* pWfxClosestMatch = NULL;

      // Set wave format
      Wfx.wFormatTag = WAVE_FORMAT_PCM;
      Wfx.wBitsPerSample = 16;
      Wfx.cbSize = 0;

      const int freqs[] = { 48000, 44100, 16000, 96000, 32000, 8000 };
      hr = S_FALSE;

      // Iterate over frequencies and channels, in order of priority
      for (int freq = 0; freq < sizeof(freqs) / sizeof(freqs[0]); freq++) {
        for (int chan = 0; chan < sizeof(m_AudioDevice->_playChannelsPrioList)
          / sizeof(m_AudioDevice->_playChannelsPrioList[0]); chan++) {
          Wfx.nChannels = m_AudioDevice->_playChannelsPrioList[chan];
          Wfx.nSamplesPerSec = freqs[freq];
          Wfx.nBlockAlign = Wfx.nChannels * Wfx.wBitsPerSample / 8;
          Wfx.nAvgBytesPerSec = Wfx.nSamplesPerSec * Wfx.nBlockAlign;
          // If the method succeeds and the audio endpoint device supports the
          // specified stream format, it returns S_OK. If the method succeeds
          // and provides a closest match to the specified format, it returns
          // S_FALSE.
          hr = audioClient->IsFormatSupported(
            AUDCLNT_SHAREMODE_SHARED,
            &Wfx,
            &pWfxClosestMatch);
          if (hr == S_OK) {
            break;
          } else {
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
              "nChannels=%d, nSamplesPerSec=%d is not supported",
              Wfx.nChannels, Wfx.nSamplesPerSec);
          }
        }
        if (hr == S_OK)
          break;
      }


      if (hr == S_OK) {
        m_AudioDevice->_playAudioFrameSize = Wfx.nBlockAlign;
        m_AudioDevice->_playBlockSize = Wfx.nSamplesPerSec / 100;
        m_AudioDevice->_playSampleRate = Wfx.nSamplesPerSec;
        // The device itself continues to run at 44.1 kHz.
        m_AudioDevice->_devicePlaySampleRate = Wfx.nSamplesPerSec;
        m_AudioDevice->_devicePlayBlockSize = Wfx.nSamplesPerSec / 100;
        m_AudioDevice->_playChannels = Wfx.nChannels;

        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
          "VoE selected this rendering format:");
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
          "wFormatTag         : 0x%X (%u)", Wfx.wFormatTag, Wfx.wFormatTag);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
          "nChannels          : %d", Wfx.nChannels);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
          "nSamplesPerSec     : %d", Wfx.nSamplesPerSec);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
          "nAvgBytesPerSec    : %d", Wfx.nAvgBytesPerSec);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
          "nBlockAlign        : %d", Wfx.nBlockAlign);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
          "wBitsPerSample     : %d", Wfx.wBitsPerSample);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
          "cbSize             : %d", Wfx.cbSize);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
          "Additional settings:");
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
          "_playAudioFrameSize: %d", m_AudioDevice->_playAudioFrameSize);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
          "_playBlockSize     : %d", m_AudioDevice->_playBlockSize);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
          "_playChannels      : %d", m_AudioDevice->_playChannels);
      } else {
        // IsFormatSupported failed, device is probably in surround mode.
        // Firstly generate mix format to initialize media engine
        WAVEFORMATEX *WfxMix = m_AudioDevice->GenerateMixFormatForMediaEngine(mixFormat);

        // Secondly initialize media engine with "expected" values
        m_AudioDevice->_playAudioFrameSize = WfxMix->nBlockAlign;
        m_AudioDevice->_playBlockSize = WfxMix->nSamplesPerSec / 100;
        m_AudioDevice->_playSampleRate = WfxMix->nSamplesPerSec;
        // The device itself continues to run at 44.1 kHz.
        m_AudioDevice->_devicePlaySampleRate = WfxMix->nSamplesPerSec;
        m_AudioDevice->_devicePlayBlockSize = WfxMix->nSamplesPerSec / 100;
        m_AudioDevice->_playChannels = WfxMix->nChannels;

        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
          "VoE has been forced to select this rendering format:");
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
          "wFormatTag         : 0x%X (%u)", WfxMix->wFormatTag, WfxMix->wFormatTag);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
          "nChannels          : %d", WfxMix->nChannels);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
          "nSamplesPerSec     : %d", WfxMix->nSamplesPerSec);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
          "nAvgBytesPerSec    : %d", WfxMix->nAvgBytesPerSec);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
          "nBlockAlign        : %d", WfxMix->nBlockAlign);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
          "wBitsPerSample     : %d", WfxMix->wBitsPerSample);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
          "cbSize             : %d", WfxMix->cbSize);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
          "Additional settings:");
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
          "_playAudioFrameSize: %d", m_AudioDevice->_playAudioFrameSize);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
          "_playBlockSize     : %d", m_AudioDevice->_playBlockSize);
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, m_AudioDevice->_id,
          "_playChannels      : %d", m_AudioDevice->_playChannels);

        // Now switch to the real supported mix format to initialize device
        m_AudioDevice->_mixFormatSurroundOut =
          m_AudioDevice->GeneratePCMMixFormat(mixFormat);

        // Set the flag to enable upmix
        m_AudioDevice->_enableUpmix = true;
      }

      // ask for minimum buffer size (default)
      REFERENCE_TIME hnsBufferDuration = 0;
      if (mixFormat->nSamplesPerSec == 44100) {
        // Ask for a larger buffer size (30ms) when using 44.1kHz as render
        // rate. There seems to be a larger risk of underruns for 44.1 compared
        // with the default rate (48kHz). When using default, we set the
        // requested buffer duration to 0, which sets the buffer to the minimum
        // size required by the engine thread. The actual buffer size can then
        // be read by GetBufferSize() and it is 20ms on most machines.
        hnsBufferDuration = 30 * 10000;
      }

      if (m_AudioDevice->ShouldUpmix()) {
        // Initialize the AudioClient in Shared Mode with the user specified
        // buffer
        hr = audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
          AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
          hnsBufferDuration,
          0,
          reinterpret_cast<WAVEFORMATEX*>(m_AudioDevice->_mixFormatSurroundOut),
          nullptr);
      } else {
        // Initialize the AudioClient in Shared Mode with the user specified
        // buffer
        hr = audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
          AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
          hnsBufferDuration,
          0,
          &Wfx,
          nullptr);
      }

      if (FAILED(hr)) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, m_AudioDevice->_id,
          "IAudioClient::Initialize() failed:");
        if (pWfxClosestMatch != NULL) {
          WEBRTC_TRACE(kTraceError, kTraceAudioDevice, m_AudioDevice->_id,
            "closest mix format: #channels=%d, samples/sec=%d, bits/sample=%d",
            pWfxClosestMatch->nChannels, pWfxClosestMatch->nSamplesPerSec,
            pWfxClosestMatch->wBitsPerSample);
        } else {
          WEBRTC_TRACE(kTraceError, kTraceAudioDevice, m_AudioDevice->_id,
            "no format suggested, hr = 0x%08X", hr);
        }
      }

      if (FAILED(hr)) {
        goto exit;
      }

      // Get the render client
      hr = audioClient->GetService(__uuidof(IAudioRenderClient),
        reinterpret_cast<void**>(&m_AudioDevice->_ptrRenderClient));
      if (FAILED(hr)) {
        goto exit;
      }
      m_AudioDevice->_ptrClientOut = audioClient;
    }
  }

exit:
  SAFE_RELEASE(punkAudioInterface);
  CoTaskMemFree(mixFormat);

  if (FAILED(hr)) {
    SAFE_RELEASE(audioClient);
    if (m_DeviceType == eInputDevice) {
      SAFE_RELEASE(m_AudioDevice->_ptrCaptureClient);
    } else if (m_DeviceType == eOutputDevice) {
      SAFE_RELEASE(m_AudioDevice->_ptrRenderClient);
    }
  }
  m_ActivateCompleted.set(hr);
  return S_OK;
}

void AudioInterfaceActivator::SetAudioDevice(
  AudioDeviceWindowsWasapi* device) {
  m_AudioDevice = device;
}

concurrency::task<Microsoft::WRL::ComPtr<IAudioClient2>>
          AudioInterfaceActivator::ActivateAudioClientAsync(
            LPCWCHAR deviceId, ActivatorDeviceType deviceType) {
  Microsoft::WRL::ComPtr<AudioInterfaceActivator> pActivator =
    Microsoft::WRL::Make<AudioInterfaceActivator>();

  Microsoft::WRL::ComPtr<IActivateAudioInterfaceAsyncOperation> pAsyncOp;
  Microsoft::WRL::ComPtr<IActivateAudioInterfaceCompletionHandler> pHandler
                                                              = pActivator;

  m_DeviceType = deviceType;

  HRESULT hr = ActivateAudioInterfaceAsync(
    deviceId,
    __uuidof(IAudioClient2),
    nullptr,
    pHandler.Get(),
    &pAsyncOp);

  if (FAILED(hr))
    throw ref new Platform::COMException(hr);


  // Wait for the activate completed event
  return create_task(pActivator->m_ActivateCompleted).then
    (
    // Once the wait is completed then pass the async operation (pAsyncOp) to
    // a lambda function which retrieves and returns the IAudioClient2
    // interface pointer
    [pAsyncOp](HRESULT activateCompletedResult) -> Microsoft::WRL::ComPtr<IAudioClient2> {
    HRESULT hr = S_OK, hr2 = S_OK;
    Microsoft::WRL::ComPtr<IUnknown> pUnk;
    // Get the audio activation result as IUnknown pointer
    hr2 = pAsyncOp->GetActivateResult(&hr, &pUnk);

    // Activation failure
    if (FAILED(hr)) {
      LOG(LS_ERROR) << "Failed to activate " <<
        (m_DeviceType == eInputDevice ? "input" :
         m_DeviceType == eOutputDevice ? "output" : "unknown") <<
        " audio device, hr=" << std::showbase << std::hex << activateCompletedResult;
      throw ref new Platform::COMException(hr);
    }
    // Failure to get activate result
    if (FAILED(hr2)) {
      LOG(LS_ERROR) << "Failed to get activation result for " <<
        (m_DeviceType == eInputDevice ? "input" :
         m_DeviceType == eOutputDevice ? "output" : "unknown") <<
        " audio device, hr=" << std::showbase << std::hex << activateCompletedResult;
      throw ref new Platform::COMException(hr2);
    }

    if (FAILED(activateCompletedResult)) {
      LOG(LS_ERROR) << "Failed to configure " <<
        (m_DeviceType == eInputDevice ? "input" :
         m_DeviceType == eOutputDevice ? "output" : "unknown") <<
        " audio device after activate, hr=" << std::showbase << std::hex << activateCompletedResult;
      throw ref new Platform::COMException(activateCompletedResult);
    }

    // Query for the activated IAudioClient2 interface
    Microsoft::WRL::ComPtr<IAudioClient2> pAudioClient2;
    hr = pUnk.As(&pAudioClient2);

    if (FAILED(hr))
      throw ref new Platform::COMException(hr);

    // Return retrieved interface
    return pAudioClient2;
  }, concurrency::task_continuation_context::use_arbitrary());
}

// ============================================================================
//                            Construction & Destruction
// ============================================================================

// ----------------------------------------------------------------------------
//  AudioDeviceWindowsWasapi() - ctor
// ----------------------------------------------------------------------------

AudioDeviceWindowsWasapi::AudioDeviceWindowsWasapi(const int32_t id) :
    _comInit(ScopedCOMInitializer::kMTA),
    _critSect(*CriticalSectionWrapper::CreateCriticalSection()),
    _volumeMutex(*CriticalSectionWrapper::CreateCriticalSection()),
    _recordingControlMutex(*CriticalSectionWrapper::CreateCriticalSection()),
    _playoutControlMutex(*CriticalSectionWrapper::CreateCriticalSection()),
    _id(id),
    _ptrAudioBuffer(NULL),
    _ptrActivator(NULL),
    _ptrClientOut(NULL),
    _ptrClientIn(NULL),
    _ptrRenderClient(NULL),
    _ptrCaptureClient(NULL),
    _ptrCaptureVolume(NULL),
    _ptrRenderSimpleVolume(NULL),
    _builtInAecEnabled(false),
    _builtInNSEnabled(false),
    _builtInAGCEnabled(false),
    _playAudioFrameSize(0),
    _playSampleRate(0),
    _playBlockSize(0),
    _playChannels(2),
    _sndCardPlayDelay(0),
    _sndCardRecDelay(0),
    _sampleDriftAt48kHz(0),
    _mixFormatSurroundOut(NULL),
    _enableUpmix(false),
    _driftAccumulator(0),
    _writtenSamples(0),
    _readSamples(0),
    _playAcc(0),
    _recAudioFrameSize(0),
    _recSampleRate(0),
    _recBlockSize(0),
    _recChannels(2),
    _hRenderSamplesReadyEvent(NULL),
    _hPlayThread(NULL),
    _hCaptureSamplesReadyEvent(NULL),
    _hRecThread(NULL),
    _hShutdownRenderEvent(NULL),
    _hShutdownCaptureEvent(NULL),
    _hRestartRenderEvent(NULL),
    _hRestartCaptureEvent(NULL),
    _hRenderStartedEvent(NULL),
    _hCaptureStartedEvent(NULL),
    _hGetCaptureVolumeThread(NULL),
    _hSetCaptureVolumeThread(NULL),
    _hSetCaptureVolumeEvent(NULL),
    _hObserverThread(NULL),
    _hObserverStartedEvent(NULL),
    _hObserverShutdownEvent(NULL),
    _hMmTask(NULL),
    _initialized(false),
    _recording(false),
    _playing(false),
    _recIsInitialized(false),
    _playIsInitialized(false),
    _speakerIsInitialized(false),
    _microphoneIsInitialized(false),
    _AGC(false),
    _playWarning(0),
    _playError(0),
    _playIsRecovering(false),
    _recWarning(0),
    _recError(0),
    _recIsRecovering(false),
    _playBufType(AudioDeviceModule::kAdaptiveBufferSize),
    _playBufDelay(80),
    _playBufDelayFixed(80),
    _usingInputDeviceIndex(false),
    _usingOutputDeviceIndex(false),
    _inputDeviceRole(AudioDeviceRole::Communications),
    _outputDeviceRole(AudioDeviceRole::Communications),
    _inputDeviceIndex(0),
    _outputDeviceIndex(0),
    _newMicLevel(0) {
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, id, "%s created",
      __FUNCTION__);
    // Create our samples ready events - we want auto reset events that start
    // in the not-signaled state. The state of an auto-reset event object
    // remains signaled until a single waiting thread is released, at which
    // time the system automatically sets the state to nonsignaled. If no
    // threads are waiting, the event object's state remains signaled.
    // (Except for _hShutdownCaptureEvent, which is used to shutdown multiple
    // threads).

    _hRenderSamplesReadyEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    _hCaptureSamplesReadyEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    _hShutdownRenderEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    _hShutdownCaptureEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    _hRestartRenderEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    _hRestartCaptureEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    _hRenderStartedEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    _hCaptureStartedEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    _hSetCaptureVolumeEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    _hObserverStartedEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    _hObserverShutdownEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

    _perfCounterFreq.QuadPart = 1;
    _perfCounterFactor = 0.0;
    _avgCPULoad = 0.0;

    // list of number of channels to use on recording side
    _recChannelsPrioList[0] = 2;  // stereo is prio 1
    _recChannelsPrioList[1] = 1;  // mono is prio 2

    // list of number of channels to use on playout side
    _playChannelsPrioList[0] = 2;  // stereo is prio 1
    _playChannelsPrioList[1] = 1;  // mono is prio 2

    _defaultDeviceWatcher = ref new DefaultAudioDeviceWatcher(this);
}

// ----------------------------------------------------------------------------
//  AudioDeviceWindowsWasapi() - dtor
// ----------------------------------------------------------------------------

AudioDeviceWindowsWasapi::~AudioDeviceWindowsWasapi() {
  WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, _id, "%s destroyed",
    __FUNCTION__);

  Terminate();

  _ptrAudioBuffer = NULL;

  if (NULL != _hRenderSamplesReadyEvent) {
    CloseHandle(_hRenderSamplesReadyEvent);
    _hRenderSamplesReadyEvent = NULL;
  }

  if (NULL != _hCaptureSamplesReadyEvent) {
    CloseHandle(_hCaptureSamplesReadyEvent);
    _hCaptureSamplesReadyEvent = NULL;
  }

  if (NULL != _hRenderStartedEvent) {
    CloseHandle(_hRenderStartedEvent);
    _hRenderStartedEvent = NULL;
  }

  if (NULL != _hCaptureStartedEvent) {
    CloseHandle(_hCaptureStartedEvent);
    _hCaptureStartedEvent = NULL;
  }

  if (NULL != _hShutdownRenderEvent) {
    CloseHandle(_hShutdownRenderEvent);
    _hShutdownRenderEvent = NULL;
  }

  if (NULL != _hShutdownCaptureEvent) {
    CloseHandle(_hShutdownCaptureEvent);
    _hShutdownCaptureEvent = NULL;
  }

  if (NULL != _hRestartRenderEvent) {
    CloseHandle(_hRestartRenderEvent);
    _hRestartRenderEvent = NULL;
  }

  if (NULL != _hRestartCaptureEvent) {
    CloseHandle(_hRestartCaptureEvent);
    _hRestartCaptureEvent = NULL;
  }

  if (NULL != _hObserverStartedEvent) {
    CloseHandle(_hObserverStartedEvent);
    _hObserverStartedEvent = NULL;
  }

  if (NULL != _hObserverShutdownEvent) {
    CloseHandle(_hObserverShutdownEvent);
    _hObserverShutdownEvent = NULL;
  }

  if (NULL != _hSetCaptureVolumeEvent) {
    CloseHandle(_hSetCaptureVolumeEvent);
    _hSetCaptureVolumeEvent = NULL;
  }

  if (NULL != _mixFormatSurroundOut)
    delete _mixFormatSurroundOut;

  delete &_critSect;
  delete &_volumeMutex;
  delete &_recordingControlMutex;
  delete &_playoutControlMutex;
}

// ============================================================================
//                                     API
// ============================================================================

// ----------------------------------------------------------------------------
//  AttachAudioBuffer
// ----------------------------------------------------------------------------

void AudioDeviceWindowsWasapi::AttachAudioBuffer(
  AudioDeviceBuffer* audioBuffer) {
  _ptrAudioBuffer = audioBuffer;

  // Inform the AudioBuffer about default settings for this implementation.
  // Set all values to zero here since the actual settings will be done by
  // InitPlayout and InitRecording later.
  _ptrAudioBuffer->SetRecordingSampleRate(0);
  _ptrAudioBuffer->SetPlayoutSampleRate(0);
  _ptrAudioBuffer->SetRecordingChannels(0);
  _ptrAudioBuffer->SetPlayoutChannels(0);
}

// ----------------------------------------------------------------------------
//  ActiveAudioLayer
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWasapi::ActiveAudioLayer(
  AudioDeviceModule::AudioLayer& audioLayer) const {
    audioLayer = AudioDeviceModule::kWindowsCoreAudio;
    return 0;
}

// ----------------------------------------------------------------------------
//  Init
// ----------------------------------------------------------------------------

AudioDeviceGeneric::InitStatus AudioDeviceWindowsWasapi::Init() {
  CriticalSectionScoped lock(&_critSect);

  if (_initialized) {
      return AudioDeviceGeneric::InitStatus::OK;
  }

  _playWarning = 0;
  _playError = 0;
  _recWarning = 0;
  _recError = 0;

  _EnumerateEndpointDevicesAll();

  _initialized = true;

  StartObserverThread();
	return AudioDeviceGeneric::InitStatus::OK;
}

// ----------------------------------------------------------------------------
//  Terminate
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWasapi::Terminate() {
  CriticalSectionScoped lock(&_critSect);

  if (!_initialized) {
      return 0;
  }

  _initialized = false;
  _speakerIsInitialized = false;
  _microphoneIsInitialized = false;
  _playing = false;
  _recording = false;

  SAFE_RELEASE(_ptrClientOut);
  SAFE_RELEASE(_ptrClientIn);
  SAFE_RELEASE(_ptrRenderClient);
  SAFE_RELEASE(_ptrCaptureClient);
  SAFE_RELEASE(_ptrCaptureVolume);
  SAFE_RELEASE(_ptrRenderSimpleVolume);

  StopObserverThread();

  return 0;
}

// ----------------------------------------------------------------------------
//  Initialized
// ----------------------------------------------------------------------------

bool AudioDeviceWindowsWasapi::Initialized() const {
  return (_initialized);
}

// ----------------------------------------------------------------------------
//  InitSpeaker
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWasapi::InitSpeaker() {
  CriticalSectionScoped lock(&_critSect);

  if (_playing) {
      return -1;
  }

  if (_usingOutputDeviceIndex) {
    int16_t nDevices = PlayoutDevices();
    if (_outputDeviceIndex > (nDevices - 1)) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
          "current device selection is invalid => unable to initialize");
        return -1;
    }
  }

  int32_t ret(0);
  _renderDevice = nullptr;

  if (_usingOutputDeviceIndex) {
    // Refresh the selected rendering endpoint device using selected device id
    _renderDevice = _GetListDevice(DeviceClass::AudioRender,
                                   _deviceIdStringOut);
    if (_renderDevice == nullptr) {
      LOG(LS_WARNING) << "Selected audio playout device not found "
        << rtc::ToUtf8(_deviceIdStringOut->Data()).c_str()
        << ", using default!";
      WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
        "selected audio playout device not found %s, using default.",
        rtc::ToUtf8(_deviceIdStringOut->Data()).c_str());
    } else {
      LOG(LS_INFO) << "Using selected audio playout device:"
        << rtc::ToUtf8(_renderDevice->Name->Data()).c_str();
      WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
        "using selected audio playout device %s.",
        rtc::ToUtf8(_renderDevice->Name->Data()).c_str());
    }
  } else {
    _renderDevice = _GetDefaultDevice(DeviceClass::AudioRender,
                                      _outputDeviceRole);
    if (_renderDevice == nullptr) {
      LOG(LS_ERROR) << "Failed to get:"
        << (_outputDeviceRole == AudioDeviceRole::Communications ? "communications" :
            _outputDeviceRole == AudioDeviceRole::Default ? "default" : "unknown")
        << " audio playout device, using default.";
      WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
        "failed to get %s audio playout device, using default.",
        _outputDeviceRole == AudioDeviceRole::Communications ? "communications" :
        _outputDeviceRole == AudioDeviceRole::Default ? "default" : "unknown");
    } else {
      LOG(LS_INFO) << "Using "
        << (_outputDeviceRole == AudioDeviceRole::Communications ? "communications" :
            _outputDeviceRole == AudioDeviceRole::Default ? "default" : "unknown")
        << " audio playout device: "
        << rtc::ToUtf8(_renderDevice->Name->Data()).c_str();
    }
  }

  if (_renderDevice == nullptr) {
    _renderDevice = _GetDefaultDevice(DeviceClass::AudioRender,
                                      AudioDeviceRole::Communications);
    if (_renderDevice != nullptr) {
      LOG(LS_ERROR) << "Using default audio playout device:"
        << rtc::ToUtf8(_renderDevice->Name->Data()).c_str();
      WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
        "using default audio playout device: %s",
        rtc::ToUtf8(_renderDevice->Name->Data()).c_str());
    }
  }

  if (ret != 0 || (_renderDevice == nullptr)) {
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
      "failed to initialize the audio playout device, no device found");
    return -1;
  }

  Concurrency::task<void>(_InitializeAudioDeviceOutAsync(_renderDevice->Id))
    .then([this](concurrency::task<void> t) {
    try {
      t.get();
      LOG(LS_INFO) << "Output audio device activated "
        << rtc::ToUtf8(_renderDevice->Name->Data()).c_str();
      WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
        "output audio device activated: %s",
        rtc::ToUtf8(_renderDevice->Name->Data()).c_str());
    } catch(Platform::Exception^ ex) {
      LOG(LS_ERROR) << "Failed to activate output audio device "
        << rtc::ToUtf8(_renderDevice->Name->Data()).c_str()
        << ", ex="
        << rtc::ToUtf8(ex->Message->Data()).c_str();
      WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
        "failed to activate output audio device %s, ex=%s",
        rtc::ToUtf8(_renderDevice->Name->Data()).c_str(),
        rtc::ToUtf8(ex->Message->Data()).c_str());
    }
  }, concurrency::task_continuation_context::use_arbitrary()).wait();

  if (_ptrAudioBuffer) {
    // Update the audio buffer with the selected parameters
    _ptrAudioBuffer->SetPlayoutSampleRate(_playSampleRate);
    _ptrAudioBuffer->SetPlayoutChannels((uint8_t)_playChannels);
  }

  if (_ptrClientOut == nullptr) {  // Initialize audio output device failed
    LOG(LS_ERROR) << "Failed to initialize the audio playout enpoint device";
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
      "failed to initialize audio playout device");
    return -1;
  }

  SAFE_RELEASE(_ptrRenderSimpleVolume);
  ret = _ptrClientOut->GetService(__uuidof(ISimpleAudioVolume),
    reinterpret_cast<void**>(&_ptrRenderSimpleVolume));
  if (ret != 0 || _ptrRenderSimpleVolume == NULL) {
      WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                  "  failed to initialize the render simple volume");
      SAFE_RELEASE(_ptrRenderSimpleVolume);
      return -1;
  }

  _speakerIsInitialized = true;

  return 0;
}

// ----------------------------------------------------------------------------
//  InitMicrophone
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWasapi::InitMicrophone() {
  CriticalSectionScoped lock(&_critSect);

  if (_recording) {
      return -1;
  }

  if (_usingInputDeviceIndex) {
    int16_t nDevices = RecordingDevices();
    if (_inputDeviceIndex > (nDevices - 1)) {
      WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
        "current device selection is invalid => unable to initialize");
      return -1;
    }
  }

  int32_t ret(0);

  _captureDevice = nullptr;
  if (_usingInputDeviceIndex) {
    // Refresh the selected capture endpoint device using selected device Id
    _captureDevice = _GetListDevice(DeviceClass::AudioCapture,
                                    _deviceIdStringIn);
    if (_captureDevice == nullptr) {
      LOG(LS_WARNING) << "Selected audio capture device not found "
        << rtc::ToUtf8(_deviceIdStringIn->Data()).c_str()
        << ", using default";
      WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
        "selected audio capture device not found %s, using default.",
        rtc::ToUtf8(_deviceIdStringIn->Data()).c_str());
    } else {
      LOG(LS_INFO) << "Using selected audio capture device:"
        << rtc::ToUtf8(_captureDevice->Name->Data()).c_str();
      WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
        "using selected audio capture device %s",
        rtc::ToUtf8(_deviceIdStringIn->Data()).c_str());
    }
  } else {
    _captureDevice = _GetDefaultDevice(DeviceClass::AudioCapture,
                                       _inputDeviceRole);
    if (_captureDevice == nullptr) {
      LOG(LS_ERROR) << "Failed to get:"
        << (_inputDeviceRole == AudioDeviceRole::Communications ? "communications" :
            _inputDeviceRole == AudioDeviceRole::Default ? "default" : "unknown")
        << " audio capture device, using default.";
      WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
        "failed to get %s audio capture device, using default.",
        _inputDeviceRole == AudioDeviceRole::Communications ? "communications" :
        _inputDeviceRole == AudioDeviceRole::Default ? "default" : "unknown");
    } else {
      LOG(LS_INFO) << "Using "
        << (_inputDeviceRole == AudioDeviceRole::Communications ? "communications" :
            _inputDeviceRole == AudioDeviceRole::Default ? "default" : "unknown")
        << " audio capture device: "
        << rtc::ToUtf8(_captureDevice->Name->Data()).c_str();
    }

  }
  if (_captureDevice == nullptr) {
    _captureDevice = _GetDefaultDevice(DeviceClass::AudioCapture,
                                       AudioDeviceRole::Communications);
    if (_captureDevice != nullptr) {
      LOG(LS_ERROR) << "Using default audio capture device:"
        << rtc::ToUtf8(_captureDevice->Name->Data()).c_str();
      WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
        "using default audio recording device %s",
        rtc::ToUtf8(_captureDevice->Name->Data()).c_str());
    }
  }

  if (ret != 0 || (_captureDevice == nullptr)) {
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
      "failed to initialize the capturing enpoint device, device not found");
    return -1;
  }

  Concurrency::task<void>(_InitializeAudioDeviceInAsync(_captureDevice->Id))
    .then([this](concurrency::task<void> t) {
    try {
      t.get();
      LOG(LS_INFO) << "Input audio device activated "
        << rtc::ToUtf8(_captureDevice->Name->Data()).c_str();
      WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
        "input audio device activated: %s",
        rtc::ToUtf8(_captureDevice->Name->Data()).c_str());
    }
    catch (Platform::Exception^ ex) {
      LOG(LS_ERROR) << "Failed to activate input audio device "
        << rtc::ToUtf8(_captureDevice->Name->Data()).c_str()
        << ", ex="
        << rtc::ToUtf8(ex->Message->Data()).c_str();
      WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
        "failed to activate input audio device %s, ex=%s",
        rtc::ToUtf8(_captureDevice->Name->Data()).c_str(),
        rtc::ToUtf8(ex->Message->Data()).c_str());
    }
  }, concurrency::task_continuation_context::use_arbitrary()).wait();

  if (_ptrAudioBuffer) {
    // Update the audio buffer with the selected parameters
    _ptrAudioBuffer->SetRecordingSampleRate(_recSampleRate);
    _ptrAudioBuffer->SetRecordingChannels((uint8_t)_recChannels);
  }

  if (_ptrClientIn == nullptr) {
    LOG(LS_ERROR) << "Failed to initialize the capturing enpoint device";
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
      "failed to initialize the capturing enpoint device");
    return -1;
  }

  ret = _ptrClientIn->GetService(__uuidof(ISimpleAudioVolume),
    reinterpret_cast<void**>(&_ptrCaptureVolume));
  if (ret != 0 || _ptrCaptureVolume == NULL) {
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
      "failed to initialize the capture volume");
    SAFE_RELEASE(_ptrCaptureVolume);
    return -1;
  }

  _microphoneIsInitialized = true;

  return 0;
}

// ----------------------------------------------------------------------------
//  SpeakerIsInitialized
// ----------------------------------------------------------------------------

bool AudioDeviceWindowsWasapi::SpeakerIsInitialized() const {
  return (_speakerIsInitialized);
}

// ----------------------------------------------------------------------------
//  MicrophoneIsInitialized
// ----------------------------------------------------------------------------

bool AudioDeviceWindowsWasapi::MicrophoneIsInitialized() const {
  return (_microphoneIsInitialized);
}

// ----------------------------------------------------------------------------
//  SpeakerVolumeIsAvailable
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWasapi::SpeakerVolumeIsAvailable(bool& available) {
  CriticalSectionScoped lock(&_critSect);

  if (_ptrClientOut == nullptr) {
    return -1;
  }

  HRESULT hr = S_OK;
  ISimpleAudioVolume* pVolume = NULL;

  hr = _ptrClientOut->GetService(__uuidof(ISimpleAudioVolume),
    reinterpret_cast<void**>(&pVolume));
  EXIT_ON_ERROR(hr);

  float volume(0.0f);
  hr = pVolume->GetMasterVolume(&volume);
  if (FAILED(hr)) {
    available = false;
  }
  available = true;

  SAFE_RELEASE(pVolume);

  return 0;

Exit:
  _TraceCOMError(hr);
  SAFE_RELEASE(pVolume);
  return -1;
}

// ----------------------------------------------------------------------------
//  SetSpeakerVolume
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWasapi::SetSpeakerVolume(uint32_t volume) {
  {
    CriticalSectionScoped lock(&_critSect);

    if (!_speakerIsInitialized) {
      return -1;
    }

    if (_ptrRenderSimpleVolume == NULL) {
      return -1;
    }
  }

  if (volume < (uint32_t)MIN_CORE_SPEAKER_VOLUME ||
      volume > (uint32_t)MAX_CORE_SPEAKER_VOLUME) {
      return -1;
  }

  HRESULT hr = S_OK;

  // scale input volume to valid range (0.0 to 1.0)
  const float fLevel = static_cast<float>(volume) / MAX_CORE_SPEAKER_VOLUME;
  _volumeMutex.Enter();
  hr = _ptrRenderSimpleVolume->SetMasterVolume(fLevel, NULL);
  _volumeMutex.Leave();
  EXIT_ON_ERROR(hr);

  return 0;

Exit:
  _TraceCOMError(hr);
  return -1;
}

// ----------------------------------------------------------------------------
//  SpeakerVolume
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWasapi::SpeakerVolume(uint32_t& volume) const {
  {
    CriticalSectionScoped lock(&_critSect);

    if (!_speakerIsInitialized) {
      return -1;
    }

    if (_ptrRenderSimpleVolume == NULL) {
      return -1;
    }
  }

  HRESULT hr = S_OK;
  float fLevel(0.0f);

  _volumeMutex.Enter();
  hr = _ptrRenderSimpleVolume->GetMasterVolume(&fLevel);
  _volumeMutex.Leave();
  EXIT_ON_ERROR(hr);

  // scale input volume range [0.0,1.0] to valid output range
  volume = static_cast<uint32_t> (fLevel*MAX_CORE_SPEAKER_VOLUME);

  return 0;

Exit:
  _TraceCOMError(hr);
  return -1;
}

// ----------------------------------------------------------------------------
//  SetWaveOutVolume
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWasapi::SetWaveOutVolume(uint16_t volumeLeft,
  uint16_t volumeRight) {
  return -1;
}

// ----------------------------------------------------------------------------
//  WaveOutVolume
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWasapi::WaveOutVolume(uint16_t& volumeLeft,
  uint16_t& volumeRight) const {
  return -1;
}
// ----------------------------------------------------------------------------
//  MaxSpeakerVolume
//
//  The internal range for Core Audio is 0.0 to 1.0, where 0.0 indicates
//  silence and 1.0 indicates full volume (no attenuation).
//  We add our (webrtc-internal) own max level to match the Wave API and
//  how it is used today in VoE.
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWasapi::MaxSpeakerVolume(uint32_t& maxVolume) const {
  if (!_speakerIsInitialized) {
      return -1;
  }

  maxVolume = static_cast<uint32_t> (MAX_CORE_SPEAKER_VOLUME);

  return 0;
}

// ----------------------------------------------------------------------------
//  MinSpeakerVolume
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWasapi::MinSpeakerVolume(uint32_t& minVolume) const {
  if (!_speakerIsInitialized) {
    return -1;
  }

  minVolume = static_cast<uint32_t> (MIN_CORE_SPEAKER_VOLUME);

  return 0;
}

// ----------------------------------------------------------------------------
//  SpeakerVolumeStepSize
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWasapi::SpeakerVolumeStepSize(
  uint16_t& stepSize) const {
  if (!_speakerIsInitialized) {
    return -1;
  }

  stepSize = CORE_SPEAKER_VOLUME_STEP_SIZE;

  return 0;
}

// ----------------------------------------------------------------------------
//  SpeakerMuteIsAvailable
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWasapi::SpeakerMuteIsAvailable(bool& available) {
  CriticalSectionScoped lock(&_critSect);

  if (_ptrClientOut == NULL) {
    return -1;
  }

  HRESULT hr = S_OK;
  ISimpleAudioVolume* pVolume = NULL;

  // Query the speaker system mute state.
  hr = _ptrClientOut->GetService(__uuidof(ISimpleAudioVolume),
    reinterpret_cast<void**>(&pVolume));
  EXIT_ON_ERROR(hr);

  BOOL mute;
  hr = pVolume->GetMute(&mute);
  if (FAILED(hr))
    available = false;
  else
    available = true;

  SAFE_RELEASE(pVolume);

  return 0;

Exit:
  _TraceCOMError(hr);
  SAFE_RELEASE(pVolume);
  return -1;
}

// ----------------------------------------------------------------------------
//  SetSpeakerMute
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWasapi::SetSpeakerMute(bool enable) {
  CriticalSectionScoped lock(&_critSect);

  if (!_speakerIsInitialized) {
    return -1;
  }

  if (_ptrClientOut == NULL) {
    return -1;
  }

  HRESULT hr = S_OK;
  ISimpleAudioVolume* pVolume = NULL;

  // Set the speaker system mute state.
  hr = _ptrClientOut->GetService(__uuidof(ISimpleAudioVolume),
    reinterpret_cast<void**>(&pVolume));
  EXIT_ON_ERROR(hr);

  const BOOL mute(enable);
  hr = pVolume->SetMute(mute, NULL);
  EXIT_ON_ERROR(hr);

  SAFE_RELEASE(pVolume);

  return 0;

Exit:
  _TraceCOMError(hr);
  SAFE_RELEASE(pVolume);
  return -1;
}

// ----------------------------------------------------------------------------
//  SpeakerMute
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWasapi::SpeakerMute(bool& enabled) const {
  if (!_speakerIsInitialized) {
      return -1;
  }

  if (_ptrClientOut == NULL) {
      return -1;
  }

  HRESULT hr = S_OK;
  ISimpleAudioVolume* pVolume = NULL;

  // Query the speaker system mute state.
  hr = _ptrClientOut->GetService(__uuidof(ISimpleAudioVolume),
    reinterpret_cast<void**>(&pVolume));
  EXIT_ON_ERROR(hr);

  BOOL mute;
  hr = pVolume->GetMute(&mute);
  EXIT_ON_ERROR(hr);

  enabled = (mute == TRUE) ? true : false;

  SAFE_RELEASE(pVolume);

  return 0;

Exit:
  _TraceCOMError(hr);
  return -1;
}

// ----------------------------------------------------------------------------
//  MicrophoneMuteIsAvailable
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWasapi::MicrophoneMuteIsAvailable(bool& available) {
  CriticalSectionScoped lock(&_critSect);

  if (_ptrClientIn == NULL) {
      return -1;
  }

  HRESULT hr = S_OK;
  ISimpleAudioVolume* pVolume = NULL;

  // Query the microphone system mute state.
  hr = _ptrClientIn->GetService(__uuidof(ISimpleAudioVolume),
    reinterpret_cast<void**>(&pVolume));
  EXIT_ON_ERROR(hr);

  BOOL mute;
  hr = pVolume->GetMute(&mute);
  if (FAILED(hr))
    available = false;
  else
    available = true;

  SAFE_RELEASE(pVolume);
  return 0;

Exit:
  _TraceCOMError(hr);
  SAFE_RELEASE(pVolume);
  return -1;
}

// ----------------------------------------------------------------------------
//  SetMicrophoneMute
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWasapi::SetMicrophoneMute(bool enable) {
  if (!_microphoneIsInitialized) {
    return -1;
  }

  if (_ptrClientIn == NULL) {
      return -1;
  }

  HRESULT hr = S_OK;
  ISimpleAudioVolume* pVolume = NULL;

  // Set the microphone system mute state.
  hr = _ptrClientIn->GetService(__uuidof(ISimpleAudioVolume),
    reinterpret_cast<void**>(&pVolume));
  EXIT_ON_ERROR(hr);

  const BOOL mute(enable);
  hr = pVolume->SetMute(mute, NULL);
  EXIT_ON_ERROR(hr);

  SAFE_RELEASE(pVolume);
  return 0;

Exit:
  _TraceCOMError(hr);
  return -1;
}

// ----------------------------------------------------------------------------
//  MicrophoneMute
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWasapi::MicrophoneMute(bool& enabled) const {
  if (!_microphoneIsInitialized) {
    return -1;
  }

  HRESULT hr = S_OK;
  ISimpleAudioVolume* pVolume = NULL;

  // Query the microphone system mute state.
  hr = _ptrClientIn->GetService(__uuidof(ISimpleAudioVolume),
    reinterpret_cast<void**>(&pVolume));
  EXIT_ON_ERROR(hr);

  BOOL mute;
  hr = pVolume->GetMute(&mute);
  EXIT_ON_ERROR(hr);

  enabled = (mute == TRUE) ? true : false;

  SAFE_RELEASE(pVolume);
  return 0;

Exit:
  SAFE_RELEASE(pVolume);
  _TraceCOMError(hr);
  return -1;
}

// ----------------------------------------------------------------------------
//  MicrophoneBoostIsAvailable
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWasapi::MicrophoneBoostIsAvailable(bool& available) {
  available = false;
  return 0;
}

// ----------------------------------------------------------------------------
//  SetMicrophoneBoost
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWasapi::SetMicrophoneBoost(bool enable) {
  if (!_microphoneIsInitialized) {
      return -1;
  }

  return -1;
}

// ----------------------------------------------------------------------------
//  MicrophoneBoost
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWasapi::MicrophoneBoost(bool& enabled) const {
  if (!_microphoneIsInitialized) {
      return -1;
  }

  return -1;
}

// ----------------------------------------------------------------------------
//  StereoRecordingIsAvailable
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWasapi::StereoRecordingIsAvailable(bool& available) {
  available = true;
  return 0;
}

// ----------------------------------------------------------------------------
//  SetStereoRecording
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWasapi::SetStereoRecording(bool enable) {
  CriticalSectionScoped lock(&_critSect);

  if (enable) {
    _recChannelsPrioList[0] = 2;    // try stereo first
    _recChannelsPrioList[1] = 1;
    _recChannels = 2;
  } else {
    _recChannelsPrioList[0] = 1;    // try mono first
    _recChannelsPrioList[1] = 2;
    _recChannels = 1;
  }

  return 0;
}

// ----------------------------------------------------------------------------
//  StereoRecording
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWasapi::StereoRecording(bool& enabled) const {
  if (_recChannels == 2)
      enabled = true;
  else
      enabled = false;

  return 0;
}

// ----------------------------------------------------------------------------
//  StereoPlayoutIsAvailable
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWasapi::StereoPlayoutIsAvailable(bool& available) {
  available = true;
  return 0;
}

// ----------------------------------------------------------------------------
//  SetStereoPlayout
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWasapi::SetStereoPlayout(bool enable) {
  CriticalSectionScoped lock(&_critSect);

  if (enable) {
    _playChannelsPrioList[0] = 2;    // try stereo first
    _playChannelsPrioList[1] = 1;
    _playChannels = 2;
  } else {
    _playChannelsPrioList[0] = 1;    // try mono first
    _playChannelsPrioList[1] = 2;
    _playChannels = 1;
  }

  return 0;
}

// ----------------------------------------------------------------------------
//  StereoPlayout
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWasapi::StereoPlayout(bool& enabled) const {
  if (_playChannels == 2)
    enabled = true;
  else
    enabled = false;

  return 0;
}

// ----------------------------------------------------------------------------
//  SetAGC
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWasapi::SetAGC(bool enable) {
  CriticalSectionScoped lock(&_critSect);
  _AGC = enable;
  return 0;
}

// ----------------------------------------------------------------------------
//  AGC
// ----------------------------------------------------------------------------

bool AudioDeviceWindowsWasapi::AGC() const {
  CriticalSectionScoped lock(&_critSect);
  return _AGC;
}

// ----------------------------------------------------------------------------
//  MicrophoneVolumeIsAvailable
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWasapi::MicrophoneVolumeIsAvailable(
  bool& available) {
  CriticalSectionScoped lock(&_critSect);

  if (_ptrClientIn == NULL) {
    return -1;
  }

  HRESULT hr = S_OK;
  ISimpleAudioVolume* pVolume = NULL;

  hr = _ptrClientIn->GetService(__uuidof(ISimpleAudioVolume),
    reinterpret_cast<void**>(&pVolume));
  EXIT_ON_ERROR(hr);

  float volume(0.0f);
  hr = pVolume->GetMasterVolume(&volume);
  if (FAILED(hr)) {
      available = false;
  }
  available = true;

  SAFE_RELEASE(pVolume);
  return 0;

Exit:
  SAFE_RELEASE(pVolume);
  _TraceCOMError(hr);
  return -1;
}

// ----------------------------------------------------------------------------
//  SetMicrophoneVolume
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWasapi::SetMicrophoneVolume(uint32_t volume) {
  WEBRTC_TRACE(kTraceStream, kTraceAudioDevice, _id,
    "AudioDeviceWindowsWasapi::SetMicrophoneVolume(volume=%u)", volume); {
    CriticalSectionScoped lock(&_critSect);

    if (!_microphoneIsInitialized) {
      return -1;
    }

    if (_ptrCaptureVolume == NULL) {
      return -1;
    }
  }

  if (volume < static_cast<uint32_t>(MIN_CORE_MICROPHONE_VOLUME) ||
    volume > static_cast<uint32_t>(MAX_CORE_MICROPHONE_VOLUME)) {
    return -1;
  }

  HRESULT hr = S_OK;
  // scale input volume to valid range (0.0 to 1.0)
  const float fLevel = static_cast<float>(volume)/MAX_CORE_MICROPHONE_VOLUME;
  _volumeMutex.Enter();
  _ptrCaptureVolume->SetMasterVolume(fLevel, NULL);
  _volumeMutex.Leave();
  EXIT_ON_ERROR(hr);

  return 0;

Exit:
  _TraceCOMError(hr);
  return -1;
}

// ----------------------------------------------------------------------------
//  MicrophoneVolume
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWasapi::MicrophoneVolume(uint32_t& volume) const {
  {
    CriticalSectionScoped lock(&_critSect);

    if (!_microphoneIsInitialized) {
      return -1;
    }

    if (_ptrCaptureVolume == NULL) {
      return -1;
    }
  }

  HRESULT hr = S_OK;
  float fLevel(0.0f);
  volume = 0;
  _volumeMutex.Enter();
  hr = _ptrCaptureVolume->GetMasterVolume(&fLevel);
  _volumeMutex.Leave();
  EXIT_ON_ERROR(hr);

  // scale input volume range [0.0,1.0] to valid output range
  volume = static_cast<uint32_t> (fLevel*MAX_CORE_MICROPHONE_VOLUME);

  return 0;

Exit:
  _TraceCOMError(hr);
  return -1;
}

// ----------------------------------------------------------------------------
//  MaxMicrophoneVolume
//
//  The internal range for Core Audio is 0.0 to 1.0, where 0.0 indicates
//  silence and 1.0 indicates full volume (no attenuation).
//  We add our (webrtc-internal) own max level to match the Wave API and
//  how it is used today in VoE.
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWasapi::MaxMicrophoneVolume(
  uint32_t& maxVolume) const {
  WEBRTC_TRACE(kTraceStream, kTraceAudioDevice, _id, "%s", __FUNCTION__);

  if (!_microphoneIsInitialized) {
    return -1;
  }

  maxVolume = static_cast<uint32_t> (MAX_CORE_MICROPHONE_VOLUME);

  return 0;
}

// ----------------------------------------------------------------------------
//  MinMicrophoneVolume
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWasapi::MinMicrophoneVolume(
  uint32_t& minVolume) const {
  if (!_microphoneIsInitialized) {
    return -1;
  }

  minVolume = static_cast<uint32_t> (MIN_CORE_MICROPHONE_VOLUME);

  return 0;
}

// ----------------------------------------------------------------------------
//  MicrophoneVolumeStepSize
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWasapi::MicrophoneVolumeStepSize(
  uint16_t& stepSize) const {
  if (!_microphoneIsInitialized) {
      return -1;
  }

  stepSize = CORE_MICROPHONE_VOLUME_STEP_SIZE;

  return 0;
}

// ----------------------------------------------------------------------------
//  PlayoutDevices
// ----------------------------------------------------------------------------

int16_t AudioDeviceWindowsWasapi::PlayoutDevices() {
  CriticalSectionScoped lock(&_critSect);

  // Refresh the list of rendering endpoint devices
  _RefreshDeviceList(DeviceClass::AudioRender);

  if (_initialized) {
    return (_DeviceListCount(DeviceClass::AudioRender));
  }

  return -1;
}

// ----------------------------------------------------------------------------
//  SetPlayoutDevice I (II)
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWasapi::SetPlayoutDevice(uint16_t index) {
  if (_playIsInitialized) {
    return -1;
  }

  // Get current number of available rendering endpoint devices and refresh the
  // rendering collection.
  UINT nDevices = PlayoutDevices();

  if (index < 0 || index > (nDevices-1)) {
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
      "device index is out of range [0,%u]", (nDevices-1));
    return -1;
  }

  CriticalSectionScoped lock(&_critSect);

  assert(_ptrRenderCollection != nullptr);

  // Select an endpoint rendering device given the specified index
  _renderDevice = nullptr;
  _deviceIdStringOut = nullptr;

  _renderDevice = _ptrRenderCollection->GetAt(index);
  _deviceIdStringOut = _renderDevice->Id;

  // Get the endpoint device's friendly-name
  if (_GetDeviceName(_renderDevice) != nullptr) {
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "friendly name: \"%S\"",
      _GetDeviceName(_renderDevice));
  }

  _usingOutputDeviceIndex = true;
  _outputDeviceIndex = index;

  return 0;
}

// ----------------------------------------------------------------------------
//  SetPlayoutDevice II (II)
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWasapi::SetPlayoutDevice(
  AudioDeviceModule::WindowsDeviceType device) {
  if (_playIsInitialized) {
    return -1;
  }

  CriticalSectionScoped lock(&_critSect);

  // Refresh the list of rendering endpoint devices
  _RefreshDeviceList(DeviceClass::AudioRender);

  //  Select an endpoint rendering device given the specified role
  _renderDevice = nullptr;
  _deviceIdStringOut = nullptr;

  if (device == AudioDeviceModule::kDefaultDevice) {
    _outputDeviceRole = AudioDeviceRole::Default;
  } else if (device == AudioDeviceModule::kDefaultCommunicationDevice) {
    _outputDeviceRole = AudioDeviceRole::Communications;
  }

  _renderDevice = _GetDefaultDevice(DeviceClass::AudioRender, _outputDeviceRole);
  _deviceIdStringOut = _renderDevice->Id;

  // Get the endpoint device's friendly-name
  std::wstring str = _GetDeviceName(_renderDevice)->Data();
  if (str.length() > 0) {
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "friendly name: \"%S\"",
      str);
  }

  _usingOutputDeviceIndex = false;

  return 0;
}

// ----------------------------------------------------------------------------
//  PlayoutDeviceName
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWasapi::PlayoutDeviceName(
  uint16_t index,
  char name[kAdmMaxDeviceNameSize],
  char guid[kAdmMaxGuidSize]) {
  bool defaultCommunicationDevice(false);
  const int16_t nDevices(PlayoutDevices());  // also updates the list of
                                             // devices

  // Special fix for the case when the user selects '-1' as index (<=> Default
  // Communication Device)
  if (index == (uint16_t)(-1)) {
    defaultCommunicationDevice = true;
    index = 0;
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
      "Default Communication endpoint device will be used");
  }

  if ((index > (nDevices-1)) || (name == NULL)) {
    return -1;
  }

  memset(name, 0, sizeof(name));

  if (guid != NULL) {
    memset(guid, 0, sizeof(guid));
  }

  CriticalSectionScoped lock(&_critSect);

  Platform::String^ deviceName = nullptr;

  // Get the endpoint device's friendly-name
  if (defaultCommunicationDevice) {
    deviceName = _GetDefaultDeviceName(DeviceClass::AudioRender);
  } else {
    deviceName = _GetListDeviceName(DeviceClass::AudioRender, index);
  }

  if (deviceName != nullptr) {
    // Convert the endpoint device's friendly-name to UTF-8
    if (WideCharToMultiByte(CP_UTF8, 0, deviceName->Data(), -1, name,
      kAdmMaxDeviceNameSize, NULL, NULL) == 0) {
      WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
        "WideCharToMultiByte(CP_UTF8) failed with error code %d",
        GetLastError());
    }
  }

  Platform::String^ deviceId = nullptr;

  // Get the endpoint ID string (uniquely identifies the device among all audio
  // endpoint devices)
  if (defaultCommunicationDevice) {
    deviceId = _GetDefaultDeviceID(DeviceClass::AudioRender);
  } else {
    deviceId = _GetListDeviceID(DeviceClass::AudioRender, index);
  }

  if (guid != NULL && deviceId != nullptr) {
    // Convert the endpoint device's ID string to UTF-8
    if (WideCharToMultiByte(CP_UTF8, 0, deviceId->Data(), -1, guid,
      kAdmMaxGuidSize, NULL, NULL) == 0) {
      WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
        "WideCharToMultiByte(CP_UTF8) failed with error code %d",
        GetLastError());
    }
  }

  int32_t ret = (deviceName != nullptr && deviceId != nullptr) ? 0 : -1;
  return ret;
}

// ----------------------------------------------------------------------------
//  RecordingDeviceName
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWasapi::RecordingDeviceName(
  uint16_t index,
  char name[kAdmMaxDeviceNameSize],
  char guid[kAdmMaxGuidSize]) {
  bool defaultCommunicationDevice(false);
  const int16_t nDevices(RecordingDevices());  // also updates the list of
                                               // devices

  // Special fix for the case when the user selects '-1' as index (<=> Default
  // Communication Device)
  if (index == (uint16_t)(-1)) {
    defaultCommunicationDevice = true;
    index = 0;
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
      "Default Communication endpoint device will be used");
  }

  if ((index > (nDevices-1)) || (name == NULL)) {
    return -1;
  }

  memset(name, 0, sizeof(name));

  if (guid != NULL) {
    memset(guid, 0, sizeof(guid));
  }

  CriticalSectionScoped lock(&_critSect);

  Platform::String^ deviceName = nullptr;

  // Get the endpoint device's friendly-name
  if (defaultCommunicationDevice) {
    deviceName = _GetDefaultDeviceName(DeviceClass::AudioCapture);
  } else {
    deviceName = _GetListDeviceName(DeviceClass::AudioCapture, index);
  }

  if (deviceName != nullptr) {
    // Convert the endpoint device's friendly-name to UTF-8
    if (WideCharToMultiByte(CP_UTF8, 0, deviceName->Data(), -1, name,
      kAdmMaxDeviceNameSize, NULL, NULL) == 0) {
      WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
        "WideCharToMultiByte(CP_UTF8) failed with error code %d",
        GetLastError());
    }
  }

  Platform::String^ deviceId = nullptr;

  // Get the endpoint ID string (uniquely identifies the device among all audio
  // endpoint devices)
  if (defaultCommunicationDevice) {
    deviceId = _GetDefaultDeviceID(DeviceClass::AudioCapture);
  } else {
    deviceId = _GetListDeviceID(DeviceClass::AudioCapture, index);
  }

  if (guid != NULL && deviceId != nullptr) {
      // Convert the endpoint device's ID string to UTF-8
    if (WideCharToMultiByte(CP_UTF8, 0, deviceId->Data(), -1, guid,
      kAdmMaxGuidSize, NULL, NULL) == 0) {
      WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
        "WideCharToMultiByte(CP_UTF8) failed with error code %d",
        GetLastError());
    }
  }

  int32_t ret = (deviceName != nullptr && deviceId != nullptr) ? 0 : -1;
  return ret;
}

// ----------------------------------------------------------------------------
//  RecordingDevices
// ----------------------------------------------------------------------------

int16_t AudioDeviceWindowsWasapi::RecordingDevices() {
  CriticalSectionScoped lock(&_critSect);

  // Refresh the list of capture endpoint devices
  _RefreshDeviceList(DeviceClass::AudioCapture);

  if (_initialized) {
    return (_DeviceListCount(DeviceClass::AudioCapture));
  }

  return -1;
}

// ----------------------------------------------------------------------------
//  SetRecordingDevice I (II)
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWasapi::SetRecordingDevice(uint16_t index) {
  if (_recIsInitialized) {
      return -1;
  }

  // Get current number of available capture endpoint devices and refresh the
  // capture collection.
  UINT nDevices = RecordingDevices();

  if (index < 0 || index > (nDevices-1)) {
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
      "device index is out of range [0,%u]", (nDevices-1));
    return -1;
  }

  CriticalSectionScoped lock(&_critSect);

  assert(_ptrCaptureCollection != nullptr);

  // Select an endpoint capture device given the specified index
  _captureDevice = nullptr;
  _deviceIdStringIn = nullptr;

  _captureDevice = _ptrCaptureCollection->GetAt(index);
  _deviceIdStringIn = _captureDevice->Id;


  // Get the endpoint device's friendly-name
  if (_GetDeviceName(_captureDevice) != nullptr) {
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "friendly name: \"%S\"",
      _GetDeviceName(_captureDevice));
  }

  _usingInputDeviceIndex = true;
  _inputDeviceIndex = index;

  return 0;
}

// ----------------------------------------------------------------------------
//  SetRecordingDevice II (II)
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWasapi::SetRecordingDevice(
  AudioDeviceModule::WindowsDeviceType device) {
  if (_recIsInitialized) {
      return -1;
  }

  CriticalSectionScoped lock(&_critSect);

  // Refresh the list of capture endpoint devices
  _RefreshDeviceList(DeviceClass::AudioCapture);

  // Select an endpoint capture device given the specified role
  _captureDevice = nullptr;
  _deviceIdStringIn = nullptr;

  if (device == AudioDeviceModule::kDefaultDevice) {
    _inputDeviceRole = AudioDeviceRole::Default;
  } else if (device == AudioDeviceModule::kDefaultCommunicationDevice) {
    _inputDeviceRole = AudioDeviceRole::Communications;
  }

  _captureDevice = _GetDefaultDevice(DeviceClass::AudioCapture, _inputDeviceRole);
  _deviceIdStringIn = _captureDevice->Id;

  Platform::String^ deviceName = nullptr;

  // Get the endpoint device's friendly-name
  if (_GetDeviceName(_captureDevice) != nullptr) {
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "friendly name: \"%S\"",
      _GetDeviceName(_captureDevice));
  }

  _usingInputDeviceIndex = false;

  return 0;
}

// ----------------------------------------------------------------------------
//  PlayoutIsAvailable
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWasapi::PlayoutIsAvailable(bool& available) {
  available = false;

  // Try to initialize the playout side
  int32_t res = InitPlayout();

  // Cancel effect of initialization
  StopPlayout();

  if (res != -1) {
    available = true;
  }

  return 0;
}

// ----------------------------------------------------------------------------
//  RecordingIsAvailable
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWasapi::RecordingIsAvailable(bool& available) {
  available = false;

  // Try to initialize the recording side
  int32_t res = InitRecording();

  // Cancel effect of initialization
  StopRecording();

  if (res != -1) {
    available = true;
  }

  return 0;
}

// ----------------------------------------------------------------------------
//  InitPlayout
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWasapi::InitPlayout() {
  CriticalSectionScoped lock(&_playoutControlMutex);
  return InitPlayoutInternal();
}

int32_t AudioDeviceWindowsWasapi::InitPlayoutInternal() {
  CriticalSectionScoped lock(&_critSect);

  if (_playing) {
      return -1;
  }

  if (_playIsInitialized) {
      return 0;
  }

  if (_renderDevice == nullptr) {
      return -1;
  }

  // Initialize the speaker (devices might have been added or removed)
  if (InitSpeaker() == -1) {
      WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
        "InitSpeaker() failed");
  }

  // Ensure that the updated rendering endpoint device is valid
  if (_renderDevice == nullptr) {
      return -1;
  }

  HRESULT hr = S_OK;
  WAVEFORMATEX* pWfxOut = NULL;
  WAVEFORMATEX Wfx = WAVEFORMATEX();
  WAVEFORMATEX* pWfxClosestMatch = NULL;

  // Create COM object with IAudioClient interface.
  if (_ptrClientOut == nullptr) {
    return -1;
  }

  // Retrieve the stream format that the audio engine uses for its internal
  // processing (mixing) of shared-mode streams.
  hr = _ptrClientOut->GetMixFormat(&pWfxOut);
  if (SUCCEEDED(hr)) {
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
      "Audio Engine's current rendering mix format:");
    // format type
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
      "wFormatTag     : 0x%X (%u)", pWfxOut->wFormatTag, pWfxOut->wFormatTag);
    // number of channels (i.e. mono, stereo...)
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "nChannels      : %d",
      pWfxOut->nChannels);
    // sample rate
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "nSamplesPerSec : %d",
      pWfxOut->nSamplesPerSec);
    // for buffer estimation
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "nAvgBytesPerSec: %d",
      pWfxOut->nAvgBytesPerSec);
    // block size of data
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "nBlockAlign    : %d",
      pWfxOut->nBlockAlign);
    // number of bits per sample of mono data
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "wBitsPerSample : %d",
      pWfxOut->wBitsPerSample);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "cbSize         : %d",
      pWfxOut->cbSize);
  }

  // Set wave format
  Wfx.wFormatTag = WAVE_FORMAT_PCM;
  Wfx.wBitsPerSample = 16;
  Wfx.cbSize = 0;

  const int freqs[] = {48000, 44100, 16000, 96000, 32000, 8000};
  hr = S_FALSE;

  // Iterate over frequencies and channels, in order of priority
  for (int freq = 0; freq < sizeof(freqs)/sizeof(freqs[0]); freq++) {
    for (int chan = 0; chan < sizeof(_playChannelsPrioList) /
      sizeof(_playChannelsPrioList[0]); chan++) {
      Wfx.nChannels = _playChannelsPrioList[chan];
      Wfx.nSamplesPerSec = freqs[freq];
      Wfx.nBlockAlign = Wfx.nChannels * Wfx.wBitsPerSample / 8;
      Wfx.nAvgBytesPerSec = Wfx.nSamplesPerSec * Wfx.nBlockAlign;
      // If the method succeeds and the audio endpoint device supports the
      // specified stream format, it returns S_OK. If the method succeeds and
      // provides a closest match to the specified format, it returns S_FALSE.
      hr = _ptrClientOut->IsFormatSupported(
                            AUDCLNT_SHAREMODE_SHARED,
                            &Wfx,
                            &pWfxClosestMatch);
      if (hr == S_OK) {
          break;
      } else {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
          "nChannels=%d, nSamplesPerSec=%d is not supported",
          Wfx.nChannels, Wfx.nSamplesPerSec);
      }
    }
    if (hr == S_OK)
      break;
  }

  // TODO(andrew): what happens in the event of failure in the above loop?
  // Is _ptrClientOut->Initialize expected to fail?
  // Same in InitRecording().
  if (hr == S_OK) {
    _playAudioFrameSize = Wfx.nBlockAlign;
    _playBlockSize = Wfx.nSamplesPerSec/100;
    _playSampleRate = Wfx.nSamplesPerSec;
    // The device itself continues to run at 44.1 kHz.
    _devicePlaySampleRate = Wfx.nSamplesPerSec;
    _devicePlayBlockSize = Wfx.nSamplesPerSec/100;
    _playChannels = Wfx.nChannels;

    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
      "VoE selected this rendering format:");
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
      "wFormatTag         : 0x%X (%u)", Wfx.wFormatTag, Wfx.wFormatTag);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
      "nChannels          : %d", Wfx.nChannels);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
      "nSamplesPerSec     : %d", Wfx.nSamplesPerSec);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
      "nAvgBytesPerSec    : %d", Wfx.nAvgBytesPerSec);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
      "nBlockAlign        : %d", Wfx.nBlockAlign);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
      "wBitsPerSample     : %d", Wfx.wBitsPerSample);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
      "cbSize             : %d", Wfx.cbSize);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
      "Additional settings:");
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
      "_playAudioFrameSize: %d", _playAudioFrameSize);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
      "_playBlockSize     : %d", _playBlockSize);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
      "_playChannels      : %d", _playChannels);
  }

  _Get44kHzDrift();

  // Create a rendering stream.
  //
  // **************************************************************************
  // For a shared-mode stream that uses event-driven buffering, the caller must
  // set both hnsPeriodicity and hnsBufferDuration to 0. The Initialize method
  // determines how large a buffer to allocate based on the scheduling period
  // of the audio engine. Although the client's buffer processing thread is
  // event driven, the basic buffer management process, as described
  // previously, is unaltered.
  // Each time the thread awakens, it should call
  // IAudioClient::GetCurrentPadding to determine how much data to write to a
  // rendering buffer or read from a capture buffer. In contrast to the two
  // buffers that the Initialize method allocates for an exclusive-mode stream
  // that uses event-driven buffering, a shared-mode stream requires a single
  // buffer.
  // **************************************************************************

  // ask for minimum buffer size (default)
  REFERENCE_TIME hnsBufferDuration = 0;
  if (_devicePlaySampleRate == 44100) {
    // Ask for a larger buffer size (30ms) when using 44.1kHz as render rate.
    // There seems to be a larger risk of underruns for 44.1 compared
    // with the default rate (48kHz). When using default, we set the requested
    // buffer duration to 0, which sets the buffer to the minimum size
    // required by the engine thread. The actual buffer size can then be
    // read by GetBufferSize() and it is 20ms on most machines.
    hnsBufferDuration = 30*10000;
  }

  if (_ptrAudioBuffer) {
    // Update the audio buffer with the selected parameters
    _ptrAudioBuffer->SetPlayoutSampleRate(_playSampleRate);
    _ptrAudioBuffer->SetPlayoutChannels((uint8_t)_playChannels);
  } else {
    // We can enter this state during CoreAudioIsSupported() when no
    // AudioDeviceImplementation has been created, hence the AudioDeviceBuffer
    // does not exist. It is OK to end up here since we don't initiate any
    // media in CoreAudioIsSupported().
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
      "AudioDeviceBuffer must be attached before streaming can start");
  }

  // Get the actual size of the shared (endpoint buffer).
  // Typical value is 960 audio frames <=> 20ms @ 48kHz sample rate.
  UINT bufferFrameCount(0);
  hr = _ptrClientOut->GetBufferSize(
                        &bufferFrameCount);
  if (SUCCEEDED(hr)) {
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
      "IAudioClient::GetBufferSize() => %u (<=> %u bytes)",
      bufferFrameCount, bufferFrameCount*_playAudioFrameSize);
  }

  // Set the event handle that the system signals when an audio buffer is ready
  // to be processed by the client.
  hr = _ptrClientOut->SetEventHandle(
                        _hRenderSamplesReadyEvent);
  // EXIT_ON_ERROR(hr);

  // Get an IAudioRenderClient interface.
  SAFE_RELEASE(_ptrRenderClient);
  hr = _ptrClientOut->GetService(
                        __uuidof(IAudioRenderClient),
                        reinterpret_cast<void**>(&_ptrRenderClient));
  EXIT_ON_ERROR(hr);

  // Mark playout side as initialized
  _playIsInitialized = true;

  CoTaskMemFree(pWfxOut);
  CoTaskMemFree(pWfxClosestMatch);

  WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
    "render side is now initialized");
  return 0;

Exit:
  _TraceCOMError(hr);
  CoTaskMemFree(pWfxOut);
  CoTaskMemFree(pWfxClosestMatch);
  SAFE_RELEASE(_ptrClientOut);
  SAFE_RELEASE(_ptrRenderClient);
  return -1;
}

// ----------------------------------------------------------------------------
//  InitRecording
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWasapi::InitRecording() {
  CriticalSectionScoped lock(&_recordingControlMutex);
  return InitRecordingInternal();
}

int32_t AudioDeviceWindowsWasapi::InitRecordingInternal() {
  CriticalSectionScoped lock(&_critSect);

  if (_recording) {
    return -1;
  }

  if (_recIsInitialized) {
    return 0;
  }

  if (QueryPerformanceFrequency(&_perfCounterFreq) == 0) {
    return -1;
  }
  _perfCounterFactor = 10000000.0 / static_cast<double>(
    _perfCounterFreq.QuadPart);

  if (_captureDevice == nullptr) {
    return -1;
  }

  // Initialize the microphone (devices might have been added or removed)
  if (InitMicrophone() == -1) {
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
      "InitMicrophone() failed");
  }

  // Ensure that the updated capturing endpoint device is valid
  if (_captureDevice == nullptr) {
    return -1;
  }

  HRESULT hr = S_OK;
  WAVEFORMATEX* pWfxIn = NULL;
  WAVEFORMATEX Wfx = WAVEFORMATEX();
  WAVEFORMATEX* pWfxClosestMatch = NULL;

  // Create COM object with IAudioClient interface.
  if (_ptrClientIn == nullptr) {
    return -1;
  }

  // Retrieve the stream format that the audio engine uses for its internal
  // processing (mixing) of shared-mode streams.
  hr = _ptrClientIn->GetMixFormat(&pWfxIn);
  if (SUCCEEDED(hr)) {
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
      "Audio Engine's current capturing mix format:");
    // format type
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
      "wFormatTag     : 0x%X (%u)", pWfxIn->wFormatTag, pWfxIn->wFormatTag);
    // number of channels (i.e. mono, stereo...)
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "nChannels      : %d",
      pWfxIn->nChannels);
    // sample rate
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "nSamplesPerSec : %d",
      pWfxIn->nSamplesPerSec);
    // for buffer estimation
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "nAvgBytesPerSec: %d",
      pWfxIn->nAvgBytesPerSec);
    // block size of data
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "nBlockAlign    : %d",
      pWfxIn->nBlockAlign);
    // number of bits per sample of mono data
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "wBitsPerSample : %d",
      pWfxIn->wBitsPerSample);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "cbSize         : %d",
      pWfxIn->cbSize);
  }

  // Set wave format
  Wfx.wFormatTag = WAVE_FORMAT_PCM;
  Wfx.wBitsPerSample = 16;
  Wfx.cbSize = 0;

  const int freqs[6] = {48000, 44100, 16000, 96000, 32000, 8000};
  hr = S_FALSE;

  // Iterate over frequencies and channels, in order of priority
  for (int freq = 0; freq < sizeof(freqs)/sizeof(freqs[0]); freq++) {
    for (int chan = 0; chan < sizeof(_recChannelsPrioList) /
      sizeof(_recChannelsPrioList[0]); chan++) {
      Wfx.nChannels = _recChannelsPrioList[chan];
      Wfx.nSamplesPerSec = freqs[freq];
      Wfx.nBlockAlign = Wfx.nChannels * Wfx.wBitsPerSample / 8;
      Wfx.nAvgBytesPerSec = Wfx.nSamplesPerSec * Wfx.nBlockAlign;
      // If the method succeeds and the audio endpoint device supports the
      // specified stream format, it returns S_OK. If the method succeeds and
      // provides a closest match to the specified format, it returns S_FALSE.
      hr = _ptrClientIn->IsFormatSupported(
                            AUDCLNT_SHAREMODE_SHARED,
                            &Wfx,
                            &pWfxClosestMatch);
      if (hr == S_OK) {
          break;
      } else {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
          "nChannels=%d, nSamplesPerSec=%d is not supported",
            Wfx.nChannels, Wfx.nSamplesPerSec);
      }
    }
    if (hr == S_OK)
        break;
  }

  if (hr == S_OK) {
    _recAudioFrameSize = Wfx.nBlockAlign;
    _recSampleRate = Wfx.nSamplesPerSec;
    _recBlockSize = Wfx.nSamplesPerSec/100;
    _recChannels = Wfx.nChannels;

    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
      "VoE selected this capturing format:");
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
      "wFormatTag        : 0x%X (%u)", Wfx.wFormatTag, Wfx.wFormatTag);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "nChannels         : %d",
      Wfx.nChannels);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "nSamplesPerSec    : %d",
      Wfx.nSamplesPerSec);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "nAvgBytesPerSec   : %d",
      Wfx.nAvgBytesPerSec);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "nBlockAlign       : %d",
      Wfx.nBlockAlign);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "wBitsPerSample    : %d",
      Wfx.wBitsPerSample);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "cbSize            : %d",
      Wfx.cbSize);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "Additional settings:");
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "_recAudioFrameSize: %d",
      _recAudioFrameSize);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "_recBlockSize     : %d",
      _recBlockSize);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "_recChannels      : %d",
      _recChannels);
  }

  if (_ptrAudioBuffer) {
    // Update the audio buffer with the selected parameters
    _ptrAudioBuffer->SetRecordingSampleRate(_recSampleRate);
    _ptrAudioBuffer->SetRecordingChannels((uint8_t)_recChannels);
  } else {
    // We can enter this state during CoreAudioIsSupported() when no
    // AudioDeviceImplementation has been created, hence the AudioDeviceBuffer
    // does not exist. It is OK to end up here since we don't initiate any
    // media in CoreAudioIsSupported().
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
      "AudioDeviceBuffer must be attached before streaming can start");
  }

  // Get the actual size of the shared (endpoint buffer).
  // Typical value is 960 audio frames <=> 20ms @ 48kHz sample rate.
  UINT bufferFrameCount(0);
  hr = _ptrClientIn->GetBufferSize(&bufferFrameCount);
  if (SUCCEEDED(hr)) {
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
      "IAudioClient::GetBufferSize() => %u (<=> %u bytes)", bufferFrameCount,
      bufferFrameCount*_recAudioFrameSize);
  }

  // Set the event handle that the system signals when an audio buffer is ready
  // to be processed by the client.
  hr = _ptrClientIn->SetEventHandle(_hCaptureSamplesReadyEvent);
  // EXIT_ON_ERROR(hr);

  // Get an IAudioCaptureClient interface.
  SAFE_RELEASE(_ptrCaptureClient);
  hr = _ptrClientIn->GetService(__uuidof(IAudioCaptureClient),
    reinterpret_cast<void**>(&_ptrCaptureClient));
  EXIT_ON_ERROR(hr);

  // Mark capture side as initialized
  _recIsInitialized = true;

  CoTaskMemFree(pWfxIn);
  CoTaskMemFree(pWfxClosestMatch);

  WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
    "capture side is now initialized");
  return 0;

Exit:
  _TraceCOMError(hr);
  CoTaskMemFree(pWfxIn);
  CoTaskMemFree(pWfxClosestMatch);
  SAFE_RELEASE(_ptrClientIn);
  SAFE_RELEASE(_ptrCaptureClient);
  return -1;
}

// ----------------------------------------------------------------------------
//  StartRecording
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWasapi::StartRecording() {
  CriticalSectionScoped critScoped(&_recordingControlMutex);
  return StartRecordingInternal();
}

int32_t AudioDeviceWindowsWasapi::StartRecordingInternal() {
  if (!_recIsInitialized) {
    return -1;
  }

  if (_hRecThread != NULL) {
    return 0;
  }

  if (_recording) {
    return 0;
  }

  {
    CriticalSectionScoped critScoped(&_critSect);

    // Create thread which will drive the capturing
    LPTHREAD_START_ROUTINE lpStartAddress = WSAPICaptureThread;

    assert(_hRecThread == NULL);
    _hRecThread = CreateThread(NULL,
                                0,
                                lpStartAddress,
                                this,
                                0,
                                NULL);
    if (_hRecThread == NULL) {
      WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                    "failed to create the recording thread");
      return -1;
    }

    // Set thread priority to highest possible
    SetThreadPriority(_hRecThread, THREAD_PRIORITY_TIME_CRITICAL);

    assert(_hGetCaptureVolumeThread == NULL);
    _hGetCaptureVolumeThread = CreateThread(NULL,
                                            0,
                                            GetCaptureVolumeThread,
                                            this,
                                            0,
                                            NULL);
    if (_hGetCaptureVolumeThread == NULL) {
      WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                    "  failed to create the volume getter thread");
      return -1;
    }

    assert(_hSetCaptureVolumeThread == NULL);
    _hSetCaptureVolumeThread = CreateThread(NULL,
                                            0,
                                            SetCaptureVolumeThread,
                                            this,
                                            0,
                                            NULL);
    if (_hSetCaptureVolumeThread == NULL) {
      WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                    "  failed to create the volume setter thread");
      return -1;
    }
  }  // critScoped

  DWORD ret = WaitForSingleObject(_hCaptureStartedEvent, 1000);
  if (ret != WAIT_OBJECT_0) {
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
      "capturing did not start up properly");
    return -1;
  }
  WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
    "capture audio stream has now started...");

  _avgCPULoad = 0.0f;
  _playAcc = 0;
  _recording = true;

  return 0;
}

// ----------------------------------------------------------------------------
//  StopRecording
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWasapi::StopRecording() {
  CriticalSectionScoped critScoped(&_recordingControlMutex);
  return StopRecordingInternal();
}

int32_t AudioDeviceWindowsWasapi::StopRecordingInternal() {
  int32_t err = 0;

  if (!_recIsInitialized) {
      return 0;
  }

  _Lock();

  if (_hRecThread == NULL) {
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
      "no capturing stream is active => close down WASAPI only");
    SAFE_RELEASE(_ptrClientIn);
    SAFE_RELEASE(_ptrCaptureClient);
    _recIsInitialized = false;
    _recording = false;
    _UnLock();
    return 0;
  }

  // Stop the driving thread...
  WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
    "closing down the webrtc_core_audio_capture_thread...");
  // Manual-reset event; it will remain signalled to stop all capture threads.
  SetEvent(_hShutdownCaptureEvent);

  _UnLock();
  DWORD ret = WaitForSingleObject(_hRecThread, 2000);
  if (ret != WAIT_OBJECT_0) {
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
      "failed to close down webrtc_core_audio_capture_thread (errCode=%d)",
      ret);
    err = -1;
  } else {
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
      "webrtc_core_audio_capture_thread is now closed");
  }

  ret = WaitForSingleObject(_hGetCaptureVolumeThread, 2000);
  if (ret != WAIT_OBJECT_0) {
    // the thread did not stop as it should
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
      "  failed to close down volume getter thread");
    err = -1;
  } else {
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
      "  volume getter thread is now closed");
  }

  ret = WaitForSingleObject(_hSetCaptureVolumeThread, 2000);
  if (ret != WAIT_OBJECT_0) {
    // the thread did not stop as it should
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
      "  failed to close down volume setter thread");
    err = -1;
  } else {
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
      "  volume setter thread is now closed");
  }
  _Lock();

  ResetEvent(_hShutdownCaptureEvent);  // Must be manually reset.

  _recIsInitialized = false;
  _recording = false;

  // These will create thread leaks in the result of an error,
  // but we can at least resume the call.
  CloseHandle(_hRecThread);
  _hRecThread = NULL;

  CloseHandle(_hGetCaptureVolumeThread);
  _hGetCaptureVolumeThread = NULL;

  CloseHandle(_hSetCaptureVolumeThread);
  _hSetCaptureVolumeThread = NULL;

  // Reset the recording delay value.
  _sndCardRecDelay = 0;

  _UnLock();

  return err;
}

// ----------------------------------------------------------------------------
//  RecordingIsInitialized
// ----------------------------------------------------------------------------

bool AudioDeviceWindowsWasapi::RecordingIsInitialized() const {
  return (_recIsInitialized);
}

// ----------------------------------------------------------------------------
//  Recording
// ----------------------------------------------------------------------------

bool AudioDeviceWindowsWasapi::Recording() const {
  return (_recording);
}

// ----------------------------------------------------------------------------
//  PlayoutIsInitialized
// ----------------------------------------------------------------------------

bool AudioDeviceWindowsWasapi::PlayoutIsInitialized() const {
  return (_playIsInitialized);
}

// ----------------------------------------------------------------------------
//  StartPlayout
// ----------------------------------------------------------------------------
int32_t AudioDeviceWindowsWasapi::StartPlayout() {
  CriticalSectionScoped critScoped(&_playoutControlMutex);
  return StartPlayoutInternal();
}

int32_t AudioDeviceWindowsWasapi::StartPlayoutInternal() {
  if (!_playIsInitialized) {
    return -1;
  }

  if (_hPlayThread != NULL) {
    return 0;
  }

  if (_playing) {
    return 0;
  }

  {
    CriticalSectionScoped critScoped(&_critSect);

    // Create thread which will drive the rendering.
    assert(_hPlayThread == NULL);
    _hPlayThread = CreateThread(
                      NULL,
                      0,
                      WSAPIRenderThread,
                      this,
                      0,
                      NULL);
    if (_hPlayThread == NULL) {
      WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
          "failed to create the playout thread");
      return -1;
    }

    // Set thread priority to highest possible.
    SetThreadPriority(_hPlayThread, THREAD_PRIORITY_TIME_CRITICAL);
  }  // critScoped

  DWORD ret = WaitForSingleObject(_hRenderStartedEvent, 1000);
  if (ret != WAIT_OBJECT_0) {
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
        "rendering did not start up properly");
    return -1;
  }

  _playing = true;
  WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
    "rendering audio stream has now started...");

  return 0;
}

// ----------------------------------------------------------------------------
//  StopPlayout
// ----------------------------------------------------------------------------
int32_t AudioDeviceWindowsWasapi::StopPlayout() {
  CriticalSectionScoped critScoped(&_playoutControlMutex);
  return StopPlayoutInternal();
}

int32_t AudioDeviceWindowsWasapi::StopPlayoutInternal() {
  if (!_playIsInitialized) {
    return 0;
  }

  {
    CriticalSectionScoped critScoped(&_critSect);

    if (_hPlayThread == NULL) {
      WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
        "no rendering stream is active => close down WASAPI only");
      SAFE_RELEASE(_ptrClientOut);
      SAFE_RELEASE(_ptrRenderClient);
      _playIsInitialized = false;
      _playing = false;
      return 0;
    }

    // stop the driving thread...
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
      "closing down the webrtc_core_audio_render_thread...");
    SetEvent(_hShutdownRenderEvent);
  }  // critScoped

  DWORD ret = WaitForSingleObject(_hPlayThread, 2000);
  if (ret != WAIT_OBJECT_0) {
    // the thread did not stop as it should
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
        "failed to close down webrtc_core_audio_render_thread");
    CloseHandle(_hPlayThread);
    _hPlayThread = NULL;
    _playIsInitialized = false;
    _playing = false;
    return -1;
  }

  {
    CriticalSectionScoped critScoped(&_critSect);
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
      "webrtc_core_audio_render_thread is now closed");

    // to reset this event manually at each time we finish with it, in case
    // that the render thread has exited before StopPlayout(), this event
    // might be caught by the new render thread within same VoE instance.
    ResetEvent(_hShutdownRenderEvent);

    SAFE_RELEASE(_ptrClientOut);
    SAFE_RELEASE(_ptrRenderClient);
    SAFE_RELEASE(_ptrRenderSimpleVolume);

    _playIsInitialized = false;
    _playing = false;

    CloseHandle(_hPlayThread);
    _hPlayThread = NULL;

    if (_builtInAecEnabled && _recording) {
        // The DMO won't provide us captured output data unless we
        // give it render data to process.
        //
        // We still permit the playout to shutdown, and trace a warning.
        // Otherwise, VoE can get into a state which will never permit
        // playout to stop properly.
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
          "Recording should be stopped before playout when using the "
          "built-in AEC");
    }

    // Reset the playout delay value.
    _sndCardPlayDelay = 0;
  }  // critScoped

  return 0;
}

// ----------------------------------------------------------------------------
//  PlayoutDelay
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWasapi::PlayoutDelay(uint16_t& delayMS) const {
  CriticalSectionScoped critScoped(&_critSect);
  delayMS = static_cast<uint16_t>(_sndCardPlayDelay);
  return 0;
}

// ----------------------------------------------------------------------------
//  RecordingDelay
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWasapi::RecordingDelay(uint16_t& delayMS) const {
  CriticalSectionScoped critScoped(&_critSect);
  delayMS = static_cast<uint16_t>(_sndCardRecDelay);
  return 0;
}

// ----------------------------------------------------------------------------
//  Playing
// ----------------------------------------------------------------------------

bool AudioDeviceWindowsWasapi::Playing() const {
  return (_playing);
}
// ----------------------------------------------------------------------------
//  SetPlayoutBuffer
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWasapi::SetPlayoutBuffer(
  const AudioDeviceModule::BufferType type, uint16_t sizeMS) {
  CriticalSectionScoped lock(&_critSect);

  _playBufType = type;

  if (type == AudioDeviceModule::kFixedBufferSize) {
      _playBufDelayFixed = sizeMS;
  }

  return 0;
}

// ----------------------------------------------------------------------------
//  PlayoutBuffer
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWasapi::PlayoutBuffer(
  AudioDeviceModule::BufferType& type, uint16_t& sizeMS) const {
  CriticalSectionScoped lock(&_critSect);
  type = _playBufType;

  if (type == AudioDeviceModule::kFixedBufferSize) {
    sizeMS = _playBufDelayFixed;
  } else {
    // Use same value as for PlayoutDelay
    sizeMS = static_cast<uint16_t>(_sndCardPlayDelay);
  }

  return 0;
}

// ----------------------------------------------------------------------------
//  CPULoad
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWasapi::CPULoad(uint16_t& load) const {
  load = static_cast<uint16_t> (100*_avgCPULoad);

  return 0;
}

// ----------------------------------------------------------------------------
//  PlayoutWarning
// ----------------------------------------------------------------------------

bool AudioDeviceWindowsWasapi::PlayoutWarning() const {
  return ( _playWarning > 0);
}

// ----------------------------------------------------------------------------
//  PlayoutError
// ----------------------------------------------------------------------------

bool AudioDeviceWindowsWasapi::PlayoutError() const {
  return ( _playError > 0);
}

// ----------------------------------------------------------------------------
//  RecordingWarning
// ----------------------------------------------------------------------------

bool AudioDeviceWindowsWasapi::RecordingWarning() const {
  return ( _recWarning > 0);
}

// ----------------------------------------------------------------------------
//  RecordingError
// ----------------------------------------------------------------------------

bool AudioDeviceWindowsWasapi::RecordingError() const {
  return ( _recError > 0);
}

// ----------------------------------------------------------------------------
//  ClearPlayoutWarning
// ----------------------------------------------------------------------------

void AudioDeviceWindowsWasapi::ClearPlayoutWarning() {
  _playWarning = 0;
}

// ----------------------------------------------------------------------------
//  ClearPlayoutError
// ----------------------------------------------------------------------------

void AudioDeviceWindowsWasapi::ClearPlayoutError() {
  _playError = 0;
}

// ----------------------------------------------------------------------------
//  ClearRecordingWarning
// ----------------------------------------------------------------------------

void AudioDeviceWindowsWasapi::ClearRecordingWarning() {
  _recWarning = 0;
}

// ----------------------------------------------------------------------------
//  ClearRecordingError
// ----------------------------------------------------------------------------

void AudioDeviceWindowsWasapi::ClearRecordingError() {
  _recError = 0;
}

// ============================================================================
//                                 Private Methods
// ============================================================================

// ----------------------------------------------------------------------------
//  [static] WSAPIRenderThread
// ----------------------------------------------------------------------------

DWORD WINAPI AudioDeviceWindowsWasapi::WSAPIRenderThread(LPVOID context) {
  return reinterpret_cast<AudioDeviceWindowsWasapi*>(context)->
    DoRenderThread();
}

// ----------------------------------------------------------------------------
//  [static] WSAPICaptureThread
// ----------------------------------------------------------------------------

DWORD WINAPI AudioDeviceWindowsWasapi::WSAPICaptureThread(LPVOID context) {
  return reinterpret_cast<AudioDeviceWindowsWasapi*>(context)->
    DoCaptureThread();
}

DWORD WINAPI AudioDeviceWindowsWasapi::GetCaptureVolumeThread(LPVOID context) {
  return reinterpret_cast<AudioDeviceWindowsWasapi*>(context)->
    DoGetCaptureVolumeThread();
}

DWORD WINAPI AudioDeviceWindowsWasapi::SetCaptureVolumeThread(LPVOID context) {
  return reinterpret_cast<AudioDeviceWindowsWasapi*>(context)->
    DoSetCaptureVolumeThread();
}

DWORD AudioDeviceWindowsWasapi::DoGetCaptureVolumeThread() {
  HANDLE waitObject = _hShutdownCaptureEvent;

  while (1) {
    if (AGC()) {
      uint32_t currentMicLevel = 0;
      if (MicrophoneVolume(currentMicLevel) == 0) {
        // This doesn't set the system volume, just stores it.
        _Lock();
        if (_ptrAudioBuffer) {
            _ptrAudioBuffer->SetCurrentMicLevel(currentMicLevel);
        }
        _UnLock();
      }
    }

    DWORD waitResult = WaitForSingleObject(waitObject,
      GET_MIC_VOLUME_INTERVAL_MS);
    switch (waitResult) {
      case WAIT_OBJECT_0:  // _hShutdownCaptureEvent
        return 0;
      case WAIT_TIMEOUT:   // timeout notification
        break;
      default:             // unexpected error
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
            "  unknown wait termination on get volume thread");
        return 1;
    }
  }
}

DWORD AudioDeviceWindowsWasapi::DoSetCaptureVolumeThread() {
  HANDLE waitArray[2] = {_hShutdownCaptureEvent, _hSetCaptureVolumeEvent};

  while (1) {
    DWORD waitResult = WaitForMultipleObjects(2, waitArray, FALSE, INFINITE);
    switch (waitResult) {
      case WAIT_OBJECT_0:      // _hShutdownCaptureEvent
        return 0;
      case WAIT_OBJECT_0 + 1:  // _hSetCaptureVolumeEvent
        break;
      default:                 // unexpected error
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
          "  unknown wait termination on set volume thread");
        return 1;
    }

    _Lock();
    uint32_t newMicLevel = _newMicLevel;
    _UnLock();

    if (SetMicrophoneVolume(newMicLevel) == -1) {
      WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
        "  the required modification of the microphone volume failed");
    }
  }
}

// ----------------------------------------------------------------------------
//  DoRenderThread
// ----------------------------------------------------------------------------

DWORD AudioDeviceWindowsWasapi::DoRenderThread() {
  bool keepPlaying = true;
  HANDLE waitArray[2] = { _hShutdownRenderEvent, _hRenderSamplesReadyEvent };
  HRESULT hr = S_OK;
  // HANDLE hMmTask = NULL;

  LARGE_INTEGER t1;
  LARGE_INTEGER t2;
  int32_t time(0);

  // Initialize COM as MTA in this thread.
  ScopedCOMInitializer comInit(ScopedCOMInitializer::kMTA);
  if (!comInit.succeeded()) {
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
      "failed to initialize COM in render thread");
    return 1;
  }

  _SetThreadName(0, "webrtc_core_audio_render_thread");

  _Lock();

  IAudioClock* clock = NULL;

  // Get size of rendering buffer (length is expressed as the number of audio
  // frames the buffer can hold).
  // This value is fixed during the rendering session.
  UINT32 bufferLength = 0;
  hr = _ptrClientOut->GetBufferSize(&bufferLength);
  EXIT_ON_ERROR(hr);
  WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
    "[REND] size of buffer       : %u", bufferLength);

  // Get maximum latency for the current stream (will not change for the
  // lifetime of the IAudioClient object).
  REFERENCE_TIME latency;
  _ptrClientOut->GetStreamLatency(&latency);
  WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
    "[REND] max stream latency   : %u (%3.2f ms)",
    (DWORD)latency, (double)(latency/10000.0));

  // Get the length of the periodic interval separating successive processing
  // passes by the audio engine on the data in the endpoint buffer.
  //
  // The period between processing passes by the audio engine is fixed for a
  // particular audio endpoint device and represents the smallest processing
  // quantum for the audio engine. This period plus the stream latency between
  // the buffer and endpoint device represents the minimum possible latency
  // that an audio application can achieve. Typical value: 100000 <=> 0.01
  // sec = 10ms.
  REFERENCE_TIME devPeriod = 0;
  REFERENCE_TIME devPeriodMin = 0;
  _ptrClientOut->GetDevicePeriod(&devPeriod, &devPeriodMin);
  WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
    "[REND] device period        : %u (%3.2f ms)", (DWORD)devPeriod,
    static_cast<double>(devPeriod / 10000.0));

  // Derive initial rendering delay.
  // Example: 10*(960/480) + 15 = 20 + 15 = 35ms
  int playout_delay = 10 * (bufferLength / _playBlockSize) +
    static_cast<int>((latency + devPeriod) / 10000);
  _sndCardPlayDelay = playout_delay;
  _writtenSamples = 0;
  WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
    "[REND] initial delay        : %u", playout_delay);

  double endpointBufferSizeMS = 10.0 * (static_cast<double>(bufferLength) /
    static_cast<double>(_devicePlayBlockSize));
  WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
    "[REND] endpointBufferSizeMS : %3.2f", endpointBufferSizeMS);

  BYTE *pData = NULL;
  // Before starting the stream, fill the rendering buffer with silence.
  {
    UINT32 initialPadding = 0;
    hr = _ptrClientOut->GetCurrentPadding(&initialPadding);
    EXIT_ON_ERROR(hr);

    // Derive the amount of available space in the output buffer
    // Is it possible to silence the padding as well?
    uint32_t initialFramesAvailable = bufferLength - initialPadding;

    hr = _ptrRenderClient->GetBuffer(initialFramesAvailable, &pData);
    EXIT_ON_ERROR(hr);

    hr = _ptrRenderClient->ReleaseBuffer(initialFramesAvailable,
      AUDCLNT_BUFFERFLAGS_SILENT);
    EXIT_ON_ERROR(hr);
  }

  _writtenSamples += bufferLength;

  hr = _ptrClientOut->GetService(__uuidof(IAudioClock),
    reinterpret_cast<void**>(&clock));
  if (FAILED(hr)) {
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
      "failed to get IAudioClock interface from the IAudioClient");
  }

  // Start up the rendering audio stream.
  hr = _ptrClientOut->Start();
  if (FAILED(hr)) {
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
      "failed to start rendering client, hr = 0x%08X", hr);
    goto Exit;
  }

  _UnLock();

  // Set event which will ensure that the calling thread modifies the playing
  // state to true.
  SetEvent(_hRenderStartedEvent);
  // >> ------------------ THREAD LOOP ------------------

  while (keepPlaying) {
    // Wait for a render notification event or a shutdown event
    DWORD waitResult = WaitForMultipleObjects(2, waitArray, FALSE, 500);
    switch (waitResult) {
    case WAIT_OBJECT_0 + 0:     // _hShutdownRenderEvent
      keepPlaying = false;
      break;
    case WAIT_OBJECT_0 + 1:     // _hRenderSamplesReadyEvent
      break;
    case WAIT_TIMEOUT:          // timeout notification
      WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
        "render event timed out after 0.5 seconds");
      goto Exit;
    default:                    // unexpected error
      WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
        "unknown wait termination on render side");
      goto Exit;
    }

    while (keepPlaying) {
      _Lock();

      // Sanity check to ensure that essential states are not modified
      // during the unlocked period.
      if (_ptrRenderClient == NULL || _ptrClientOut == NULL) {
          _UnLock();
          WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id,
            "output state has been modified during unlocked period");
          goto Exit;
      }

      // Get the number of frames of padding (queued up to play) in the
      // endpoint buffer.
      UINT32 padding = 0;
      hr = _ptrClientOut->GetCurrentPadding(&padding);
      if (FAILED(hr)) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
          "rendering loop failed (GetCurrentPadding), hr = 0x%08X", hr);
        goto Exit;
      }

      // Derive the amount of available space in the output buffer
      uint32_t framesAvailable = bufferLength - padding;
      // WEBRTC_TRACE(kTraceStream, kTraceAudioDevice, _id,
      //   "#avaliable audio frames = %u", framesAvailable);

      // Do we have 10 ms available in the render buffer?
      if (framesAvailable < _playBlockSize) {
        // Not enough space in render buffer to store next render packet.
        _UnLock();
        break;
      }

      // Write n*10ms buffers to the render buffer
      const uint32_t n10msBuffers = (framesAvailable / _playBlockSize);
      for (uint32_t n = 0; n < n10msBuffers; n++) {
        // Get pointer (i.e., grab the buffer) to next space in the shared
        // render buffer.
        hr = _ptrRenderClient->GetBuffer(_playBlockSize, &pData);
        if (FAILED(hr)) {
          WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
            "rendering loop failed (GetBuffer), hr = 0x%08X", hr);
          goto Exit;
        }

        QueryPerformanceCounter(&t1);    // measure time: START

        if (_ptrAudioBuffer) {
          // Request data to be played out (#bytes =
          // _playBlockSize*_audioFrameSize)
          _UnLock();
          int32_t nSamples =
          _ptrAudioBuffer->RequestPlayoutData(_playBlockSize);
          _Lock();

          if (nSamples == -1) {
            _UnLock();
            WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id,
              "failed to read data from render client");
            goto Exit;
          }

          // Sanity check to ensure that essential states are not modified
          // during the unlocked period
          if (_ptrRenderClient == NULL || _ptrClientOut == NULL) {
            _UnLock();
            WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id,
              "output state has been modified during unlocked period");
            goto Exit;
          }
          if (nSamples != static_cast<int32_t>(_playBlockSize)) {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
              "nSamples(%d) != _playBlockSize(%d)", nSamples, _playBlockSize);
          }

          if (ShouldUpmix()) {
            int size = _playBlockSize * _playAudioFrameSize;
            // Create temporary array for upmixing procedure
            std::unique_ptr<BYTE> mediaEngineRenderData(new BYTE[size]);

            // Get the actual (stored) data
            nSamples = _ptrAudioBuffer->GetPlayoutData(
              reinterpret_cast<int8_t*>(mediaEngineRenderData.get()));

            if (_mixFormatSurroundOut->SubFormat == KSDATAFORMAT_SUBTYPE_PCM) {
              // Do the upmixing. We are using 16-bit samples only at this point
              Upmix(reinterpret_cast<int16_t*>(mediaEngineRenderData.get()),
                _playBlockSize,
                reinterpret_cast<int16_t*>(pData),
                _playChannels,
                _mixFormatSurroundOut->Format.nChannels);
            } else if (_mixFormatSurroundOut->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) {
              // Do the upmixing. We are using 32-bit samples only at this point
              UpmixAndConvert(reinterpret_cast<int16_t*>(mediaEngineRenderData.get()),
                _playBlockSize,
                reinterpret_cast<float*>(pData),
                _playChannels,
                _mixFormatSurroundOut->Format.nChannels);
            } else {
              WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id,
                "audio data format type is not supported");
              goto Exit;
            }
          } else {
            // Get the actual (stored) data
            nSamples = _ptrAudioBuffer->GetPlayoutData(
              reinterpret_cast<int8_t*>(pData));
          }
        }

        QueryPerformanceCounter(&t2);    // measure time: STOP
        time = static_cast<int>(t2.QuadPart - t1.QuadPart);
        _playAcc += time;

        DWORD dwFlags(0);
        hr = _ptrRenderClient->ReleaseBuffer(_playBlockSize, dwFlags);
        // See http://msdn.microsoft.com/en-us/library/dd316605(VS.85).aspx
        // for more details regarding AUDCLNT_E_DEVICE_INVALIDATED.
        if (FAILED(hr)) {
          WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
            "rendering loop failed (ReleaseBuffer), hr = 0x%08X", hr);
          goto Exit;
        }

        _writtenSamples += _playBlockSize;
      }

      // Check the current delay on the playout side.
      if (clock) {
        UINT64 pos = 0;
        UINT64 freq = 1;
        clock->GetPosition(&pos, NULL);
        clock->GetFrequency(&freq);
        playout_delay = ROUND((static_cast<double>(_writtenSamples) /
          _devicePlaySampleRate - static_cast<double>(pos) / freq) * 1000.0);
        _sndCardPlayDelay = playout_delay;
      }

      // Clear flag marking a successfull recovery.
      if (_playIsRecovering) {
        _playIsRecovering = false;
      }
      _UnLock();
    }
  }

  // ------------------ THREAD LOOP ------------------ <<

  SleepMs(static_cast<DWORD>(endpointBufferSizeMS+0.5));
  hr = _ptrClientOut->Stop();

Exit:
  SAFE_RELEASE(clock);

  if (FAILED(hr)) {
    _ptrClientOut->Stop();
    _UnLock();
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
      "rendering terminated with error, hr = 0x%08X", hr);
    _TraceCOMError(hr);
  }

  // if (_winSupportAvrt) {
  //    if (NULL != hMmTask) {
  //        _PAvRevertMmThreadCharacteristics(hMmTask);
  //    }
  // }

  _Lock();

  if (keepPlaying) {
    // In case of AUDCLNT_E_DEVICE_INVALIDATED, restart the rendering.
    // https://msdn.microsoft.com/en-us/library/windows/desktop/dd316605(v=vs.85).aspx
    bool isRecoverableError = hr == AUDCLNT_E_DEVICE_INVALIDATED;

    if (_ptrClientOut != NULL) {
      hr = _ptrClientOut->Stop();
      if (FAILED(hr)) {
        WEBRTC_TRACE(kTraceError, kTraceUtility, _id, "failed to stop rendering client, hr = 0x%08X", hr);
        _TraceCOMError(hr);
      }
      hr = _ptrClientOut->Reset();
      if (FAILED(hr)) {
        WEBRTC_TRACE(kTraceError, kTraceUtility, _id, "failed to reset rendering client, hr = 0x%08X", hr);
        _TraceCOMError(hr);
      }
    }

    if (isRecoverableError) {
      if (_playIsRecovering) {
        // If the AUDCLNT_E_DEVICE_INVALIDATED error is received right
        // after a recovery, consider it as a failure and avoid another
        // recovery.
        WEBRTC_TRACE(kTraceError, kTraceUtility, _id, "kPlayoutError message "
          "posted: rendering thread has ended pre-maturely after recovery");
        _playIsRecovering = false;
        _playError = 1;
      } else {
        WEBRTC_TRACE(kTraceWarning, kTraceUtility, _id,
          "audio rendering thread has ended pre-maturely, restarting renderer...");
        SetEvent(_hRestartRenderEvent);
      }
    } else {
      // Trigger callback from module process thread
      WEBRTC_TRACE(kTraceError, kTraceUtility, _id, "kPlayoutError message "
        "posted: rendering thread has ended pre-maturely");
      _playError = 1;
    }
  } else {
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
      "_Rendering thread is now terminated properly");
  }

  _UnLock();

  return (DWORD)hr;
}

DWORD AudioDeviceWindowsWasapi::InitCaptureThreadPriority() {
  _hMmTask = NULL;

  _SetThreadName(0, "webrtc_core_audio_capture_thread");

  return S_OK;
}

void AudioDeviceWindowsWasapi::RevertCaptureThreadPriority() {
  _hMmTask = NULL;
}

// ----------------------------------------------------------------------------
//  DoCaptureThread
// ----------------------------------------------------------------------------

DWORD AudioDeviceWindowsWasapi::DoCaptureThread() {
  bool keepRecording = true;
  HANDLE waitArray[2] = {_hShutdownCaptureEvent, _hCaptureSamplesReadyEvent};
  HRESULT hr = S_OK;

  LARGE_INTEGER t1;
  LARGE_INTEGER t2;
  int32_t time(0);

  BYTE* syncBuffer = NULL;
  UINT32 syncBufIndex = 0;

  _readSamples = 0;

  // Initialize COM as MTA in this thread.
  ScopedCOMInitializer comInit(ScopedCOMInitializer::kMTA);
  if (!comInit.succeeded()) {
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
      "failed to initialize COM in capture thread");
    return 1;
  }

  hr = InitCaptureThreadPriority();
  if (FAILED(hr)) {
    return hr;
  }

  _Lock();

  // Get size of capturing buffer (length is expressed as the number of audio
  // frames the buffer can hold). This value is fixed during the capturing
  // session.
  UINT32 bufferLength = 0;
  if (_ptrClientIn == NULL) {
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
      "input state has been modified before capture loop starts.");
    return 1;
  }
  hr = _ptrClientIn->GetBufferSize(&bufferLength);
  EXIT_ON_ERROR(hr);
  WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
    "[CAPT] size of buffer       : %u", bufferLength);

  // Allocate memory for sync buffer.
  // It is used for compensation between native 44.1 and internal 44.0 and
  // for cases when the capture buffer is larger than 10ms.
  const UINT32 syncBufferSize = 2*(bufferLength * _recAudioFrameSize);
  syncBuffer = new BYTE[syncBufferSize];
  if (syncBuffer == NULL) {
      return (DWORD)E_POINTER;
  }
  WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
    "[CAPT] size of sync buffer  : %u [bytes]", syncBufferSize);

  // Get maximum latency for the current stream (will not change for the
  // lifetime of the IAudioClient object).
  REFERENCE_TIME latency;
  _ptrClientIn->GetStreamLatency(&latency);
  WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
    "[CAPT] max stream latency   : %u (%3.2f ms)", (DWORD)latency,
    static_cast<double>(latency / 10000.0));

  // Get the length of the periodic interval separating successive processing
  // passes by the audio engine on the data in the endpoint buffer.
  REFERENCE_TIME devPeriod = 0;
  REFERENCE_TIME devPeriodMin = 0;
  _ptrClientIn->GetDevicePeriod(&devPeriod, &devPeriodMin);
  WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
    "[CAPT] device period        : %u (%3.2f ms)", (DWORD)devPeriod,
    static_cast<double>(devPeriod / 10000.0));

  double extraDelayMS = static_cast<double>((latency + devPeriod) / 10000.0);
  WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
    "[CAPT] extraDelayMS         : %3.2f", extraDelayMS);

  double endpointBufferSizeMS = 10.0 * (static_cast<double>(bufferLength) /
    static_cast<double>(_recBlockSize));
  WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
    "[CAPT] endpointBufferSizeMS : %3.2f", endpointBufferSizeMS);

  // Start up the capturing stream.
  hr = _ptrClientIn->Start();
  if (FAILED(hr)) {
    WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id,
      "failed to start capture hr = %d", hr);
    goto Exit;
  }

  _UnLock();

  // Set event which will ensure that the calling thread modifies the recording
  // state to true.
  SetEvent(_hCaptureStartedEvent);

  // >> ---------------------------- THREAD LOOP ----------------------------

  while (keepRecording) {
    // Wait for a capture notification event or a shutdown event
    DWORD waitResult = WaitForMultipleObjects(2, waitArray, FALSE, 500);
    switch (waitResult) {
    case WAIT_OBJECT_0 + 0:        // _hShutdownCaptureEvent
        keepRecording = false;
        break;
    case WAIT_OBJECT_0 + 1:        // _hCaptureSamplesReadyEvent
        break;
    case WAIT_TIMEOUT:            // timeout notification
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
          "capture event timed out after 0.5 seconds");
        goto Exit;
    default:                    // unexpected error
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
          "unknown wait termination on capture side");
        goto Exit;
    }

    while (keepRecording) {
      BYTE *pData = 0;
      UINT32 framesAvailable = 0;
      DWORD flags = 0;
      UINT64 recTime = 0;
      UINT64 recPos = 0;

      _Lock();

      // Sanity check to ensure that essential states are not modified
      // during the unlocked period.
      if (_ptrCaptureClient == NULL || _ptrClientIn == NULL) {
        _UnLock();
        WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id,
          "input state has been modified during unlocked period");
        goto Exit;
      }

      //  Find out how much capture data is available
      hr = _ptrCaptureClient->GetBuffer(
        &pData,            // packet which is ready to be read by used
        &framesAvailable,  // #frames in the captured packet (can be zero)
        &flags,            // support flags (check)
        &recPos,           // device position of first audio frame in data
                           // packet
        &recTime);         // value of performance counter at the time of
                           // recording the first audio frame

      if (SUCCEEDED(hr)) {
        if (AUDCLNT_S_BUFFER_EMPTY == hr) {
          // Buffer was empty => start waiting for a new capture notification
          // event
          _UnLock();
          break;
        }

        if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
          // Treat all of the data in the packet as silence and ignore the
          // actual data values.
          WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
            "AUDCLNT_BUFFERFLAGS_SILENT");
          pData = NULL;
        }

        assert(framesAvailable != 0);

        if (pData) {
          CopyMemory(&syncBuffer[syncBufIndex*_recAudioFrameSize], pData,
            framesAvailable*_recAudioFrameSize);
        } else {
          ZeroMemory(&syncBuffer[syncBufIndex*_recAudioFrameSize],
            framesAvailable*_recAudioFrameSize);
        }
        assert(syncBufferSize >= (syncBufIndex*_recAudioFrameSize) +
          framesAvailable * _recAudioFrameSize);

        // Release the capture buffer
        hr = _ptrCaptureClient->ReleaseBuffer(framesAvailable);
        if (FAILED(hr)) {
          WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id,
            "failed fo release capture buffer hr = %d", hr);
          goto Exit;
        }

        _readSamples += framesAvailable;
        syncBufIndex += framesAvailable;

        QueryPerformanceCounter(&t1);

        // Get the current recording and playout delay.
        uint32_t sndCardRecDelay = (uint32_t)
          (((((UINT64)t1.QuadPart * _perfCounterFactor) - recTime) /
            10000) + (10 * syncBufIndex) / _recBlockSize - 10);
        uint32_t sndCardPlayDelay =
          static_cast<uint32_t>(_sndCardPlayDelay);

        _sndCardRecDelay = sndCardRecDelay;

        while (syncBufIndex >= _recBlockSize) {
          if (_ptrAudioBuffer) {
            _ptrAudioBuffer->SetRecordedBuffer((const int8_t*)syncBuffer,
              _recBlockSize);

            _driftAccumulator += _sampleDriftAt48kHz;
            const int32_t clockDrift =
              static_cast<int32_t>(_driftAccumulator);
            _driftAccumulator -= clockDrift;

            _ptrAudioBuffer->SetVQEData(sndCardPlayDelay,
                                        sndCardRecDelay,
                                        clockDrift);

            _ptrAudioBuffer->SetTypingStatus(KeyPressed());

            QueryPerformanceCounter(&t1);    // measure time: START

            _UnLock();  // release lock while making the callback
            _ptrAudioBuffer->DeliverRecordedData();
            _Lock();    // restore the lock

            QueryPerformanceCounter(&t2);    // measure time: STOP

            // Measure "average CPU load".
            // Basically what we do here is to measure how many percent of
            // our 10ms period is used for encoding and decoding. This
            // value shuld be used as a warning indicator only and not seen
            // as an absolute value. Running at ~100% will lead to bad QoS.
            time = static_cast<int>(t2.QuadPart - t1.QuadPart);
            _avgCPULoad = static_cast<float>(_avgCPULoad*.99 +
              (time + _playAcc) /
              static_cast<double>(_perfCounterFreq.QuadPart));
            _playAcc = 0;

            // Sanity check to ensure that essential states are not
            // modified during the unlocked period
            if (_ptrCaptureClient == NULL || _ptrClientIn == NULL) {
              _UnLock();
              WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id,
                "input state has been modified during unlocked period");
              goto Exit;
            }
          }

          // store remaining data which was not able to deliver as 10ms segment
          MoveMemory(&syncBuffer[0],
            &syncBuffer[_recBlockSize*_recAudioFrameSize],
            (syncBufIndex - _recBlockSize)*_recAudioFrameSize);
          syncBufIndex -= _recBlockSize;
          sndCardRecDelay -= 10;
        }

        if (_AGC) {
          uint32_t newMicLevel = _ptrAudioBuffer->NewMicLevel();
          if (newMicLevel != 0) {
            // The VQE will only deliver non-zero microphone levels when a
            // change is needed. Set this new mic level (received from the
            // observer as return value in the callback).
            WEBRTC_TRACE(kTraceStream, kTraceAudioDevice, _id,
              "AGC change of volume: new=%u", newMicLevel);
            // We store this outside of the audio buffer to avoid
            // having it overwritten by the getter thread.
            _newMicLevel = newMicLevel;
            SetEvent(_hSetCaptureVolumeEvent);
          }
        }
      } else {
        // If GetBuffer returns AUDCLNT_E_BUFFER_ERROR, the thread consuming
        // the audio samples must wait for the next processing pass. The client
        // might benefit from keeping a count of the failed GetBuffer calls.
        // If GetBuffer returns this error repeatedly, the client can start a
        // new processing loop after shutting down the current client by
        // calling IAudioClient::Stop, IAudioClient::Reset, and releasing the
        // audio client.
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
          R"(IAudioCaptureClient::GetBuffer returned hr = 0x%08X)",
          hr);
        goto Exit;
      }

      // Clear flag marking a successfull recovery.
      if (_recIsRecovering) {
        _recIsRecovering = false;
      }
      _UnLock();
    }
  }

  // ---------------------------- THREAD LOOP ---------------------------- <<

  if (_ptrClientIn) {
    hr = _ptrClientIn->Stop();
  }

Exit:
  if (FAILED(hr)) {
    _ptrClientIn->Stop();
    _UnLock();
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
      "capturing terminated with error, hr = 0x%08X", hr);
    _TraceCOMError(hr);
  }

  RevertCaptureThreadPriority();

  _Lock();

  if (keepRecording) {
    // In case of AUDCLNT_E_DEVICE_INVALIDATED, restart the capturing.
    // https://msdn.microsoft.com/en-us/library/windows/desktop/dd316605(v=vs.85).aspx
    bool isRecoverableError = hr == AUDCLNT_E_DEVICE_INVALIDATED;
    if (_ptrClientIn != NULL) {
      hr = _ptrClientIn->Stop();
      if (FAILED(hr)) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
          "failed to stop audio capturing, hr = 0x%08X", hr);
        _TraceCOMError(hr);
      }
      hr = _ptrClientIn->Reset();
      if (FAILED(hr)) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
          "failed to reset audio capturing, hr = 0x%08X", hr);
        _TraceCOMError(hr);
      }
    }
    if (isRecoverableError) {
      if (_recIsRecovering) {
        // If the AUDCLNT_E_DEVICE_INVALIDATED error is received right
        // after a recovery, consider it as a failure and avoid another
        // recovery.
        WEBRTC_TRACE(kTraceError, kTraceUtility, _id, "kRecordingError message "
          "posted: capturing thread has ended pre-maturely after recovery");
        _recIsRecovering = false;
        _recError = 1;
      } else {
        WEBRTC_TRACE(kTraceWarning, kTraceUtility, _id, "capturing thread has "
          "ended pre-maturely, restarting capturer...");
        SetEvent(_hRestartCaptureEvent);
      }
    } else {
      WEBRTC_TRACE(kTraceError, kTraceUtility, _id, "kRecordingError message "
        "posted: capturing thread has ended pre-maturely");
      // Trigger callback from module process thread
      _recError = 1;
    }
  } else {
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
      "_Capturing thread is now terminated properly");
  }

  SAFE_RELEASE(_ptrClientIn);
  SAFE_RELEASE(_ptrCaptureClient);
  SAFE_RELEASE(_ptrCaptureVolume);

  _UnLock();

  if (syncBuffer) {
    delete [] syncBuffer;
  }

  return (DWORD)hr;
}

int32 AudioDeviceWindowsWasapi::StartObserverThread() {
  if (_hObserverThread != NULL) {
    return 0;  // Already started.
  }

  // Create thread which will restart renderer or capturer if needed.
  LPTHREAD_START_ROUTINE lpStartAddress = WSAPIObserverThread;
  assert(_hObserverThread == NULL);
  _hObserverThread = CreateThread(NULL,
    0,
    lpStartAddress,
    this,
    0,
    NULL);
  if (_hObserverThread == NULL) {
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
      "failed to create the observer thread");
    return -1;
  }

  DWORD ret = WaitForSingleObject(_hObserverStartedEvent, 1000);
  if (ret != WAIT_OBJECT_0) {
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
      "observer did not start up properly");
    return -1;
  }
  WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
    "audio observer has now started");
  return 0;
}

int32 AudioDeviceWindowsWasapi::StopObserverThread() {
  if (_hObserverThread == NULL) {
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
      "no observer thread was started");
    return 0;
  }
  WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
    "closing down the audio observer thead...");

  SetEvent(_hObserverShutdownEvent);

  DWORD ret = WaitForSingleObject(_hObserverThread, 2000);
  if (ret != WAIT_OBJECT_0) {
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
      "failed to close down audio observer thread (errCode=%d)", ret);

    ResetEvent(_hObserverShutdownEvent);  // Must be manually reset.

    // These will create thread leaks in the result of an error,
    // but we can reinitialize this module.
    CloseHandle(_hObserverThread);
    _hObserverThread = NULL;
    return -1;
  } else {
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
      "audio observer thead is now closed");
  }

  ResetEvent(_hObserverShutdownEvent);  // Must be manually reset.
  CloseHandle(_hObserverThread);
  _hObserverThread = NULL;

  return 0;
}

DWORD WINAPI AudioDeviceWindowsWasapi::WSAPIObserverThread(LPVOID context) {
  return reinterpret_cast<AudioDeviceWindowsWasapi*>(context)->
    DoObserverThread();
}

DWORD AudioDeviceWindowsWasapi::DoObserverThread() {
  _SetThreadName(0, "webrtc_core_audio_observer_thread");
  SetEvent(_hObserverStartedEvent);
  bool keepObserving = true;
  HANDLE waitArray[3] = { _hObserverShutdownEvent,
                          _hRestartCaptureEvent,
                          _hRestartRenderEvent };
  while (keepObserving) {
    // Wait shutdown or restart capturer/renderer events
    DWORD waitResult = WaitForMultipleObjects(3, waitArray, FALSE, INFINITE);
    switch (waitResult) {
    case WAIT_OBJECT_0 + 0:        // _hObserverShutdownEvent
      keepObserving = false;
      break;
    case WAIT_OBJECT_0 + 1: {      // _hRestartCaptureEvent
      CriticalSectionScoped critScoped(&_recordingControlMutex);
      WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
        "observer -> restart audio capture event detected");
      int32_t result = StopRecordingInternal();
      if (result == 0) {
        _recIsRecovering = true;
        result = InitRecordingInternal();
      }
      if (result == 0) {
        result = StartRecordingInternal();
      }
      if (result != 0) {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
          "failed to restart audio capture");
        if (_recIsRecovering) {
          // Stop recording thread in case is running
          StopRecordingInternal();
          _recIsRecovering = false;
        }
        // Trigger callback from module process thread
        _recError = 2;
      } else {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
          "audio capture restarted");
      }
      ResetEvent(_hRestartCaptureEvent);
      break;
    }
    case WAIT_OBJECT_0 + 2: {      // _hRestartRenderEvent
      CriticalSectionScoped critScoped(&_playoutControlMutex);
      int32_t result = StopPlayoutInternal();
      if (result == 0) {
        _playIsRecovering = true;
        result = InitPlayoutInternal();
      }
      if (result == 0) {
        result = StartPlayoutInternal();
      }
      if (result != 0) {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
          "failed to restart audio renderer");
        if (_playIsRecovering) {
          // Stop playout thread in case is running
          StopPlayoutInternal();
          _playIsRecovering = false;
        }
        // Trigger callback from module process thread
        _playError = 2;
      } else {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
          "audio renderer restarted");
      }
      ResetEvent(_hRestartRenderEvent);
      break;
    }

    default:                       // unexpected error
      WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
        "audio device observer unknown wait termination");
      break;
    }
  }
  WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
    "audio device observer thread terminated");
  return 0;
}

int32_t AudioDeviceWindowsWasapi::EnableBuiltInAEC(bool enable) {
  if (_recIsInitialized) {
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
        "Attempt to set Windows AEC with recording already initialized");
    return -1;
  }

  _builtInAecEnabled = enable;
  return 0;
}

bool AudioDeviceWindowsWasapi::BuiltInAECIsEnabled() const {
  return _builtInAecEnabled;
}

bool AudioDeviceWindowsWasapi::BuiltInAECIsAvailable() const {
  // There is a bug in the OS preventing the Effects detection (Noise SUppression and AEC) to work for Win10 Phones.
  // The bug is severe enough that it's not only the detection that doesn't work but the activation of the effect.
  // For Windows phone (until the bug is solved at the OS level, it will return false, and the software AEC will be used
  return CheckBuiltInCaptureCapability(Windows::Media::Effects::AudioEffectType::AcousticEchoCancellation);
}

int32_t AudioDeviceWindowsWasapi::EnableBuiltInNS(bool enable) {
  if (_recIsInitialized) {
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
      "Attempt to set Windows Noise Suppression with recording already initialized");
    return -1;
  }

  _builtInNSEnabled = enable;
  return 0;
}

bool AudioDeviceWindowsWasapi::BuiltInNSIsAvailable() const {
  return CheckBuiltInCaptureCapability(Windows::Media::Effects::AudioEffectType::NoiseSuppression);
}

int32_t AudioDeviceWindowsWasapi::EnableBuiltInAGC(bool enable) {
  if (_playIsInitialized) {
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
      "Attempt to set Windows Automatic Gain Control with playout already initialized");
    return -1;
  }

  _builtInAGCEnabled = enable;
  return 0;
}

bool AudioDeviceWindowsWasapi::BuiltInAGCIsAvailable() const {
  return CheckBuiltInRenderCapability(Windows::Media::Effects::AudioEffectType::AutomaticGainControl);
}

bool AudioDeviceWindowsWasapi::CheckBuiltInCaptureCapability(Windows::Media::Effects::AudioEffectType effect) const {

  // Check to see if the current device supports the capability

  Windows::Media::Effects::AudioCaptureEffectsManager^ effManager;

  Windows::Media::Capture::MediaCategory Category;
  Category = Windows::Media::Capture::MediaCategory::Communications;

  Platform::String^ deviceId;

  if (_deviceIdStringIn != nullptr) {
    deviceId = _deviceIdStringIn;
  } else {
    deviceId = _captureDevice->Id;
  }

  effManager = Windows::Media::Effects::AudioEffectsManager::CreateAudioCaptureEffectsManager(
        deviceId, Category, Windows::Media::AudioProcessing::Default);

  Windows::Foundation::Collections::IVectorView<Windows::Media::Effects::AudioEffect^>^ effectsList;

  effectsList = effManager->GetAudioCaptureEffects();

  unsigned int i;
  // Iterate through the supported effect to see if Echo Cancellation is supported
  for (i = 0; i < effectsList->Size; i++) {
    if (effectsList->GetAt(i)->AudioEffectType == effect) {
      return true;
    }
  }
  return false;
}

bool AudioDeviceWindowsWasapi::CheckBuiltInRenderCapability(Windows::Media::Effects::AudioEffectType effect) const {
  Windows::Media::Effects::AudioRenderEffectsManager^ effManager;

  Windows::Media::Render::AudioRenderCategory Category;
  Category = Windows::Media::Render::AudioRenderCategory::Communications;

  Platform::String^ deviceId;

  if (_deviceIdStringOut != nullptr) {
    deviceId = _deviceIdStringOut;
  } else {
    deviceId = _renderDevice->Id;
  }

  try {
    effManager = Windows::Media::Effects::AudioEffectsManager::CreateAudioRenderEffectsManager(
      deviceId, Category, Windows::Media::AudioProcessing::Default);
  } catch (Platform::Exception^ ex) {
    LOG(LS_ERROR) << "Failed to create audio render effects manager ("
      << rtc::ToUtf8(ex->Message->Data()).c_str() << ")";
    return false;
  }

  Windows::Foundation::Collections::IVectorView<Windows::Media::Effects::AudioEffect^>^ effectsList;

  effectsList = effManager->GetAudioRenderEffects();

  unsigned int i;
  for (i = 0; i < effectsList->Size; ++i) {
    if (effectsList->GetAt(i)->AudioEffectType == effect) {
      return true;
    }
  }
  return false;
}



int AudioDeviceWindowsWasapi::SetBoolProperty(IPropertyStore* ptrPS,
                                          REFPROPERTYKEY key,
                                          VARIANT_BOOL value) {
  PROPVARIANT pv;
  PropVariantInit(&pv);
  pv.vt = VT_BOOL;
  pv.boolVal = value;
  HRESULT hr = ptrPS->SetValue(key, pv);
  PropVariantClear(&pv);
  if (FAILED(hr)) {
    _TraceCOMError(hr);
    return -1;
  }
  return 0;
}

int AudioDeviceWindowsWasapi::SetVtI4Property(IPropertyStore* ptrPS,
                                          REFPROPERTYKEY key,
                                          LONG value) {
  PROPVARIANT pv;
  PropVariantInit(&pv);
  pv.vt = VT_I4;
  pv.lVal = value;
  HRESULT hr = ptrPS->SetValue(key, pv);
  PropVariantClear(&pv);
  if (FAILED(hr)) {
    _TraceCOMError(hr);
    return -1;
  }
  return 0;
}

// ----------------------------------------------------------------------------
//  _RefreshDeviceList
//
//  Creates a new list of endpoint rendering or capture devices after
//  deleting any previously created (and possibly out-of-date) list of
//  such devices.
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWasapi::_RefreshDeviceList(DeviceClass cls) {
  WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", __FUNCTION__);

  try {
    Concurrency::task<DeviceInformationCollection^>(DeviceInformation::
      FindAllAsync(cls)).then([this](DeviceInformationCollection^ interfaces) {
      _ptrCollection = interfaces;
    }, concurrency::task_continuation_context::use_arbitrary()).wait();
  }
  catch (Platform::InvalidArgumentException^) {
    // The InvalidArgumentException gets thrown by FindAllAsync when the GUID
    // isn't formatted properly. The only reason we're catching it here is
    // because the user is allowed to enter GUIDs without validation In normal
    // usage of the API, this exception handling probably wouldn't be necessary
    // when using known-good GUIDs
  }

  if (cls == DeviceClass::AudioCapture) {
    _ptrCaptureCollection = _ptrCollection;
  } else if (cls == DeviceClass::AudioRender) {
    _ptrRenderCollection = _ptrCollection;
  } else {
    return -1;
  }

  return 0;
}

// ----------------------------------------------------------------------------
//  _DeviceListCount
//
//  Gets a count of the endpoint rendering or capture devices in the
//  current list of such devices.
// ----------------------------------------------------------------------------

int16_t AudioDeviceWindowsWasapi::_DeviceListCount(DeviceClass cls) {
  WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", __FUNCTION__);

  UINT count = 0;

  if (cls == DeviceClass::AudioCapture) {
    count = _ptrCaptureCollection->Size;
  } else if (cls == DeviceClass::AudioRender) {
    count = _ptrRenderCollection->Size;
  } else {
    return -1;
  }

  return static_cast<int16_t> (count);
}

// ----------------------------------------------------------------------------
//  _GetListDeviceName
//
//  Gets the friendly name of an endpoint rendering or capture device
//  from the current list of such devices. The caller uses an index
//  into the list to identify the device.
//
//  Uses: _ptrRenderCollection or _ptrCaptureCollection which is updated
//  in _RefreshDeviceList().
// ----------------------------------------------------------------------------

Platform::String^ AudioDeviceWindowsWasapi::_GetListDeviceName(
  DeviceClass cls, int index) {
  WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", __FUNCTION__);

  if (cls == DeviceClass::AudioRender) {
    return _ptrRenderCollection->GetAt(index)->Name;
  } else if (cls == DeviceClass::AudioCapture) {
    return _ptrCaptureCollection->GetAt(index)->Name;;
  }

  return nullptr;
}

// ----------------------------------------------------------------------------
//  _GetDefaultDeviceName
//
//  Gets the friendly name of an endpoint rendering or capture device
//  given a specified device role.
//
//  Uses: _ptrEnumerator
// ----------------------------------------------------------------------------

Platform::String^ AudioDeviceWindowsWasapi::_GetDefaultDeviceName(
  DeviceClass cls) {
  WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", __FUNCTION__);

  if (cls == DeviceClass::AudioRender) {
    DeviceInformation^ defaultDevice = _GetDefaultDevice(
      DeviceClass::AudioRender, AudioDeviceRole::Default);
    return defaultDevice != nullptr ? defaultDevice->Name : nullptr;
  } else if (cls == DeviceClass::AudioCapture) {
    DeviceInformation^ defaultDevice = _GetDefaultDevice(
      DeviceClass::AudioCapture, AudioDeviceRole::Default);
    return defaultDevice != nullptr ? defaultDevice->Name : nullptr;
  }

  return nullptr;
}

// ----------------------------------------------------------------------------
//  _GetListDeviceID
//
//  Gets the unique ID string of an endpoint rendering or capture device
//  from the current list of such devices. The caller uses an index
//  into the list to identify the device.
//
//  Uses: _ptrRenderCollection or _ptrCaptureCollection which is updated
//  in _RefreshDeviceList().
// ----------------------------------------------------------------------------

Platform::String^ AudioDeviceWindowsWasapi::_GetListDeviceID(DeviceClass cls,
  int index) {
  WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", __FUNCTION__);

  if (cls == DeviceClass::AudioRender) {
    return _ptrRenderCollection->GetAt(index)->Id;
  } else if (cls == DeviceClass::AudioCapture) {
    return _ptrCaptureCollection->GetAt(index)->Id;
  }

  return nullptr;
}

// ----------------------------------------------------------------------------
//  _GetDefaultDeviceID
//
//  Gets the uniqe device ID of an endpoint rendering or capture device
//  given a specified device role.
//
//  Uses: _ptrEnumerator
// ----------------------------------------------------------------------------

Platform::String^ AudioDeviceWindowsWasapi::_GetDefaultDeviceID(
  DeviceClass cls) {
  WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", __FUNCTION__);

  if (cls == DeviceClass::AudioRender) {
    DeviceInformation^ defaultDevice = _GetDefaultDevice(
      DeviceClass::AudioRender, AudioDeviceRole::Default);
    return defaultDevice != nullptr ? defaultDevice->Id : nullptr;
  } else if (cls == DeviceClass::AudioCapture) {
    DeviceInformation^ defaultDevice = _GetDefaultDevice(
      DeviceClass::AudioCapture, AudioDeviceRole::Default);
    return defaultDevice != nullptr ? defaultDevice->Id : nullptr;
  }

  return nullptr;
}

// ----------------------------------------------------------------------------
//  _GetDeviceName
// ----------------------------------------------------------------------------

Platform::String^ AudioDeviceWindowsWasapi::_GetDeviceName(
  DeviceInformation^ device) {
  WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", __FUNCTION__);

  return device->Name;
}

// ----------------------------------------------------------------------------
//  _GetDeviceID
// ----------------------------------------------------------------------------

Platform::String^ AudioDeviceWindowsWasapi::_GetDeviceID(
  DeviceInformation^ device) {
  WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", __FUNCTION__);

  return device->Id;
}

// ----------------------------------------------------------------------------
//  _GetDefaultDevice
// ----------------------------------------------------------------------------

DeviceInformation^ AudioDeviceWindowsWasapi::_GetDefaultDevice(
  DeviceClass cls, AudioDeviceRole role) {
  WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", __FUNCTION__);
  if (cls == DeviceClass::AudioRender) {
    DeviceInformation^ defaultRenderDevice = nullptr;
    Concurrency::create_task(
      Windows::Devices::Enumeration::DeviceInformation::CreateFromIdAsync(
      MediaDevice::GetDefaultAudioRenderId(role))).then(
      [this, &defaultRenderDevice](Windows::Devices::Enumeration::DeviceInformation^
      deviceInformation) {
      defaultRenderDevice = deviceInformation;
    }, concurrency::task_continuation_context::use_arbitrary()).wait();
    return defaultRenderDevice;
  } else if (cls == DeviceClass::AudioCapture) {
    DeviceInformation^ defaultCaptureDevice = nullptr;
    Concurrency::create_task(Windows::Devices::Enumeration::
      DeviceInformation::CreateFromIdAsync(MediaDevice::
      GetDefaultAudioCaptureId(role))).then(
      [this, &defaultCaptureDevice](Windows::Devices::Enumeration::DeviceInformation^
      deviceInformation) {
      defaultCaptureDevice = deviceInformation;
    }, concurrency::task_continuation_context::use_arbitrary()).wait();
    return defaultCaptureDevice;
  }
  return nullptr;
}

// ----------------------------------------------------------------------------
//  _GetListDevice
// ----------------------------------------------------------------------------

DeviceInformation^ AudioDeviceWindowsWasapi::_GetListDevice(DeviceClass cls,
  int index) {
  if (cls == DeviceClass::AudioRender) {
    return _ptrRenderCollection->GetAt(index);
  } else if (cls == DeviceClass::AudioCapture) {
    return _ptrCaptureCollection->GetAt(index);
  }

  return nullptr;
}

// ----------------------------------------------------------------------------
//  _GetListDevice
// ----------------------------------------------------------------------------

DeviceInformation^ AudioDeviceWindowsWasapi::_GetListDevice(DeviceClass cls,
  Platform::String^ deviceId) {
  if (deviceId == nullptr) {
    return nullptr;
  }

  if (cls == DeviceClass::AudioRender) {
    for (unsigned int i = 0; i < _ptrRenderCollection->Size; ++i) {
      DeviceInformation^ device = _ptrRenderCollection->GetAt(i);
      if (device->Id == deviceId) {
        return device;
      }
    }
  } else if (cls == DeviceClass::AudioCapture) {
    for (unsigned int i = 0; i < _ptrCaptureCollection->Size; ++i) {
      DeviceInformation^ device = _ptrCaptureCollection->GetAt(i);
      if (device->Id == deviceId) {
        return device;
      }
    }
  }

  return nullptr;
}

// ----------------------------------------------------------------------------
//  _EnumerateEndpointDevicesAll
// ----------------------------------------------------------------------------

int32_t AudioDeviceWindowsWasapi::_EnumerateEndpointDevicesAll() {
  WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", __FUNCTION__);

  HRESULT hr = S_OK;

  // Generate a collection of audio endpoint devices in the system.
  // Get states for *AudioCapture* endpoint devices.
  try {
    Concurrency::task<DeviceInformationCollection^>(
      DeviceInformation::FindAllAsync(DeviceClass::AudioCapture)).then(
      [this](concurrency::task<DeviceInformationCollection^> getDevicesTask) {
      try {
        _ptrCaptureCollection = getDevicesTask.get();
      } catch (Platform::Exception^ ex) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
          "Failed to enumerate audio capture devices, ex=%s", rtc::ToUtf8(ex->Message->Data()).c_str());
      }
    }, concurrency::task_continuation_context::use_arbitrary()).wait();
  }
  catch (Platform::InvalidArgumentException^) {
    // The InvalidArgumentException gets thrown by FindAllAsync when the GUID
    // isn't formatted properly. The only reason we're catching it here is
    // because the user is allowed to enter GUIDs without validation. In normal
    // usage of the API, this exception handling probably wouldn't be necessary
    // when using known-good GUIDs
  }

  // Generate a collection of audio endpoint devices in the system.
  // Get states for *AudioRender* endpoint devices.
  try {
    Concurrency::task<DeviceInformationCollection^>(
      DeviceInformation::FindAllAsync(DeviceClass::AudioRender)).then(
      [this](concurrency::task<DeviceInformationCollection^> getDevicesTask) {
      try {
        _ptrRenderCollection = getDevicesTask.get();
      }
      catch (Platform::Exception^ ex) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
          "Failed to enumerate audio render devices, ex=%s", rtc::ToUtf8(ex->Message->Data()));
      }
    }, concurrency::task_continuation_context::use_arbitrary()).wait();
  }
  catch (Platform::InvalidArgumentException^) {
    // The InvalidArgumentException gets thrown by FindAllAsync when the GUID
    // isn't formatted properly. The only reason we're catching it here is
    // because the user is allowed to enter GUIDs without validation. In normal
    // usage of the API, this exception handling probably wouldn't be necessary
    // when using known-good GUIDs
  }

  EXIT_ON_ERROR(hr);
  return 0;

Exit:
  _TraceCOMError(hr);
    return -1;
}

//
//  InitializeAudioDeviceAsync()
//
//  Activates the default audio capture on a asynchronous callback thread.
//  This needs to be called from the main UI thread.
//

Windows::Foundation::IAsyncAction^ AudioDeviceWindowsWasapi::
_InitializeAudioDeviceInAsync(Platform::String^ deviceId) {
  return Concurrency::create_async([this] {
    _InitializeAudioDeviceIn(_captureDevice->Id);
  });
}

HRESULT AudioDeviceWindowsWasapi::_InitializeAudioDeviceIn(Platform::String^ deviceId) {
  HRESULT hr = S_OK;

  AudioInterfaceActivator::SetAudioDevice(this);
  Platform::Exception^ ex = nullptr;
  AudioInterfaceActivator::ActivateAudioClientAsync(
    deviceId->Data(),
    AudioInterfaceActivator::ActivatorDeviceType::eInputDevice).then(
  [deviceId](Microsoft::WRL::ComPtr<IAudioClient2> captureClient) {
    return Windows::Devices::Enumeration::DeviceInformation::
      CreateFromIdAsync(deviceId);
  }, concurrency::task_continuation_context::use_arbitrary()).then(
  [this, deviceId, &ex](concurrency::task<DeviceInformation^> t) {
    try {
      t.get();
    } catch (Platform::Exception^ e) {
      ex = e;
      WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
        "failed to activate input audio device id=%s ex=%s",
        rtc::ToUtf8(deviceId->Data()).c_str(),
        rtc::ToUtf8(e->Message->Data()).c_str());
    }
  }, concurrency::task_continuation_context::use_arbitrary()).wait();
  if (ex != nullptr)
    throw ex;
  return hr;
}

Windows::Foundation::IAsyncAction^ AudioDeviceWindowsWasapi::
_InitializeAudioDeviceOutAsync(Platform::String^ deviceId) {
  return Concurrency::create_async([this] {
    _InitializeAudioDeviceOut(_renderDevice->Id);
  });
}

HRESULT AudioDeviceWindowsWasapi::_InitializeAudioDeviceOut(Platform::String^ deviceId) {
  HRESULT hr = S_OK;

  AudioInterfaceActivator::SetAudioDevice(this);
  Platform::Exception^ ex = nullptr;
  AudioInterfaceActivator::ActivateAudioClientAsync(
    deviceId->Data(),
    AudioInterfaceActivator::ActivatorDeviceType::eOutputDevice).then(
  [deviceId](Microsoft::WRL::ComPtr<IAudioClient2> renderClient) {
      return Windows::Devices::Enumeration::DeviceInformation::
        CreateFromIdAsync(deviceId);
    }, concurrency::task_continuation_context::use_arbitrary()).then(
  [this, deviceId, &ex](concurrency::task<DeviceInformation^> t) {
    try {
      t.get();
    }
    catch (Platform::Exception^ e) {
      ex = e;
      WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
        "failed to activate output audio device id=%s ex=%s",
        rtc::ToUtf8(deviceId->Data()).c_str(),
        rtc::ToUtf8(e->Message->Data()).c_str());
    }
  }, concurrency::task_continuation_context::use_arbitrary()).wait();

  if (ex != nullptr)
    throw ex;
  return hr;
}

// ----------------------------------------------------------------------------
//  ShouldUpmix
// ----------------------------------------------------------------------------
bool AudioDeviceWindowsWasapi::ShouldUpmix() {
  return _enableUpmix;
}

// ----------------------------------------------------------------------------
//  GenerateMixFormatForMediaEngine
// ----------------------------------------------------------------------------
WAVEFORMATEX* AudioDeviceWindowsWasapi::GenerateMixFormatForMediaEngine(
  WAVEFORMATEX* actualMixFormat) {
  WAVEFORMATEX* Wfx = new WAVEFORMATEX();

  bool isStereo = false;
  StereoPlayoutIsAvailable(isStereo);

  // Set wave format
  Wfx->wFormatTag = WAVE_FORMAT_PCM;
  Wfx->wBitsPerSample = 16;
  Wfx->cbSize = 0;

  Wfx->nChannels = isStereo ? 2 : 1;
  Wfx->nSamplesPerSec = actualMixFormat->nSamplesPerSec;
  Wfx->nBlockAlign = Wfx->nChannels * Wfx->wBitsPerSample / 8;
  Wfx->nAvgBytesPerSec = Wfx->nSamplesPerSec * Wfx->nBlockAlign;

  return Wfx;
}

// ----------------------------------------------------------------------------
//  GeneratePCMMixFormat
//  Main principles used from
//  https://msdn.microsoft.com/en-us/library/windows/hardware/dn653308%28v=vs.85%29.aspx
// ----------------------------------------------------------------------------
WAVEFORMATPCMEX* AudioDeviceWindowsWasapi::GeneratePCMMixFormat(
  WAVEFORMATEX* actualMixFormat) {
  WAVEFORMATPCMEX *waveFormatPCMEx = new WAVEFORMATPCMEX();

  waveFormatPCMEx->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
  waveFormatPCMEx->Format.nChannels = actualMixFormat->nChannels;
  waveFormatPCMEx->Format.wBitsPerSample = actualMixFormat->wBitsPerSample;
  waveFormatPCMEx->Format.nSamplesPerSec = actualMixFormat->nSamplesPerSec;

  waveFormatPCMEx->Format.nBlockAlign = waveFormatPCMEx->Format.nChannels *
    waveFormatPCMEx->Format.wBitsPerSample / 8;  /* Same as the usual */

  waveFormatPCMEx->Format.nAvgBytesPerSec =
    waveFormatPCMEx->Format.nSamplesPerSec * waveFormatPCMEx->Format.nBlockAlign;

  waveFormatPCMEx->Format.cbSize = 22;  /* After this to GUID */
  waveFormatPCMEx->Samples.wValidBitsPerSample =
    waveFormatPCMEx->Format.wBitsPerSample;  /* All bits have data */

  switch (waveFormatPCMEx->Format.nChannels) {
  case 1:
    waveFormatPCMEx->dwChannelMask = KSAUDIO_SPEAKER_MONO;
    break;
  case 2:
    waveFormatPCMEx->dwChannelMask = KSAUDIO_SPEAKER_STEREO;
    break;
  case 4:
    waveFormatPCMEx->dwChannelMask = KSAUDIO_SPEAKER_QUAD;
    break;
  case 6:
    waveFormatPCMEx->dwChannelMask = KSAUDIO_SPEAKER_5POINT1;
    break;
  case 8:
    waveFormatPCMEx->dwChannelMask = KSAUDIO_SPEAKER_7POINT1;
    break;
  default:
    waveFormatPCMEx->dwChannelMask = KSAUDIO_SPEAKER_STEREO;
    break;
  }

  if (waveFormatPCMEx->Format.wBitsPerSample == 16) {
    waveFormatPCMEx->SubFormat = KSDATAFORMAT_SUBTYPE_PCM;  // Specify PCM
  } else if (waveFormatPCMEx->Format.wBitsPerSample == 32) {
    waveFormatPCMEx->SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;  // Specify FLOAT
  } else {
    WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id,
      "wrong value for number of bits per sample");
    return NULL;
  }

  return waveFormatPCMEx;
}

// ----------------------------------------------------------------------------
//  Upmix
//  Reference upmixer application found on
//  https://hg.mozilla.org/releases/mozilla-aurora/file/tip/media/libcubeb/src/cubeb_wasapi.cpp
// ----------------------------------------------------------------------------
void AudioDeviceWindowsWasapi::Upmix(
                                     int16_t *inSamples,
                                     uint32_t numberOfFrames,
                                     int16_t *outSamplesReal,
                                     uint32_t inChannels,
                                     uint32_t outChannels) {
  // Create temporary array to do the upmix
  std::unique_ptr<int16_t> outSamples(new int16_t[numberOfFrames * outChannels]);

  // Copy over input channels
  for (uint32_t i = 0, o = 0; i < numberOfFrames * inChannels;
    i += inChannels, o += outChannels) {
    if (inChannels <= outChannels) {
      for (uint32_t j = 0; j < inChannels; ++j) {
        outSamples.get()[o + j] = inSamples[i + j];
      }
    } else {
      for (uint32_t j = 0; j < outChannels; ++j) {
        outSamples.get()[o + j] = (inSamples[i + j] + inSamples[i + j + 1]) / 2;
      }
    }
  }

  if (inChannels < outChannels) {
    // Add 0 to other channels
    for (uint32_t i = 0, o = 0; i < numberOfFrames; ++i, o += outChannels) {
      for (uint32_t j = inChannels; j < outChannels; ++j) {
        outSamples.get()[o + j] = 0;
      }
    }
  }

  // Copy over memory to be delivered to the IAudioRenderClient
  memcpy(outSamplesReal, outSamples.get(),
    _playBlockSize * outChannels * sizeof(int16_t));
}

void AudioDeviceWindowsWasapi::UpmixAndConvert(
                                               int16_t *inSamples,
                                               uint32_t numberOfFrames,
                                               float *outSamplesReal,
                                               uint32_t inChannels,
                                               uint32_t outChannels) {
  // Create temporary array to do the upmix
  std::unique_ptr<float> outSamples(new float[numberOfFrames * outChannels]);

  // Copy over input channels
  for (uint32_t i = 0, o = 0; i < numberOfFrames * inChannels;
    i += inChannels, o += outChannels) {
    if (inChannels <= outChannels) {
      for (uint32_t j = 0; j < inChannels; ++j) {
        outSamples.get()[o + j] = (float)inSamples[i + j] / (float)INT16_MAX;
      }
    } else {
      for (uint32_t j = 0; j < outChannels; ++j) {
        outSamples.get()[o + j] = ((float)inSamples[i + j] + (float)inSamples[i + j + 1]) / (float)INT16_MAX / 2.0;
      }
    }
  }

  if (inChannels < outChannels) {
    // Add 0 to other channels
    for (uint32_t i = 0, o = 0; i < numberOfFrames; ++i, o += outChannels) {
      for (uint32_t j = inChannels; j < outChannels; ++j) {
        outSamples.get()[o + j] = 0.0f;
      }
    }
  }

  // Copy over memory to be delivered to the IAudioRenderClient
  memcpy(outSamplesReal, outSamples.get(),
    _playBlockSize * outChannels * sizeof(float));
}


// ----------------------------------------------------------------------------
//  _TraceCOMError
// ----------------------------------------------------------------------------

void AudioDeviceWindowsWasapi::_TraceCOMError(HRESULT hr) const {
  TCHAR errorText[MAXERRORLENGTH];

  const DWORD dwFlags = FORMAT_MESSAGE_FROM_SYSTEM |
    FORMAT_MESSAGE_IGNORE_INSERTS;
  const DWORD dwLangID = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);

  // Gets the system's human readable message string for this HRESULT.
  // All error message in English by default.
  DWORD messageLength = ::FormatMessageW(dwFlags,
    0,
    hr,
    dwLangID,
    errorText,
    MAXERRORLENGTH,
    NULL);

  assert(messageLength <= MAXERRORLENGTH);

  // Trims tailing white space (FormatMessage() leaves a trailing cr-lf.).
  for (; messageLength && ::isspace(errorText[messageLength - 1]);
    --messageLength) {
    errorText[messageLength - 1] = '\0';
  }

  WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
    "Core Audio method failed (hr=0x%08X)", hr);
  if (messageLength) {
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "Error details (length=%d): %s", messageLength, rtc::ToUtf8(errorText, messageLength));
  } else {
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "Error details (length=%d): N/A", messageLength);
  }
}


// ----------------------------------------------------------------------------
//  _SetThreadName
// ----------------------------------------------------------------------------

void AudioDeviceWindowsWasapi::_SetThreadName(DWORD dwThreadID,
  LPCSTR szThreadName) {
  // See http://msdn.microsoft.com/en-us/library/xcb2z8hs(VS.71).aspx for
  // details on the code in this function. Name of article is "Setting a Thread
  // Name (Unmanaged)".

  THREADNAME_INFO info;
  info.dwType = 0x1000;
  info.szName = szThreadName;
  info.dwThreadID = dwThreadID;
  info.dwFlags = 0;
}

// ----------------------------------------------------------------------------
//  _Get44kHzDrift
// ----------------------------------------------------------------------------

void AudioDeviceWindowsWasapi::_Get44kHzDrift() {
  // We aren't able to resample at 44.1 kHz. Instead we run at 44 kHz and
  // push/pull from the engine faster to compensate. If only one direction is
  // set to 44.1 kHz the result is indistinguishable from clock drift to the
  // AEC. We can compensate internally if we inform the AEC about the drift.
  _sampleDriftAt48kHz = 0;
  _driftAccumulator = 0;

  if (_playSampleRate == 44000 && _recSampleRate != 44000) {
    _sampleDriftAt48kHz = 480.0f / 440;
  } else if (_playSampleRate != 44000 && _recSampleRate == 44000) {
    _sampleDriftAt48kHz = -480.0f / 441;
  }
}

bool AudioDeviceWindowsWasapi::KeyPressed() const {
  int key_down = 0;
  return (key_down > 0);
}

void AudioDeviceWindowsWasapi::DefaultAudioCaptureDeviceChanged(
  DefaultAudioCaptureDeviceChangedEventArgs^ args) {
  if (_usingInputDeviceIndex) {
    // Not using default audio input device
    return;
  }
  if (!Recording()) {
    return;
  }
  if (_inputDeviceRole != args->Role) {
    return;
  }
  LOG(LS_INFO) << "Default audio capture device changed, restarting capturer!";
  SetEvent(_hRestartCaptureEvent);
}

void AudioDeviceWindowsWasapi::DefaultAudioRenderDeviceChanged(
  DefaultAudioRenderDeviceChangedEventArgs^ args) {
  if (_usingOutputDeviceIndex) {
    // Not using default audio output device
    return;
  }
  if (!Playing()) {
    return;
  }
  if (_outputDeviceRole != args->Role) {
    return;
  }
  LOG(LS_INFO) << "Default audio render device changed, restarting renderer!";
  SetEvent(_hRestartRenderEvent);
}

}  // namespace webrtc

#endif  // WEBRTC_WINDOWS_CORE_AUDIO_BUILD
