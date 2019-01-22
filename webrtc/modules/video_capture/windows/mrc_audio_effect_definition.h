#ifndef WEBRTC_MODULES_VIDEO_CAPTURE_WINDOWS_MRC_AUDIO_EFFECT_DEFINITION_H_
#define WEBRTC_MODULES_VIDEO_CAPTURE_WINDOWS_MRC_AUDIO_EFFECT_DEFINITION_H_

// This class provides an IAudioEffectDefinition which can be used
// to configure and create a MixedRealityCaptureAudioEffect
// object. See https://developer.microsoft.com/en-us/windows/holographic/mixed_reality_capture_for_developers#creating_a_custom_mixed_reality_capture_.28mrc.29_recorder
// for more information about the effect definition properties.

#define RUNTIMECLASS_MIXEDREALITYCAPTURE_AUDIO_EFFECT L"Windows.Media.MixedRealityCapture.MixedRealityCaptureAudioEffect"

//
// MixerMode
// Type: AudioMixerMode as UINT32
//  0: Mic audio only
//  1: System audio only
//  2: Mixing mic and system audio (default)
//
#define PROPERTY_MIXERMODE  L"MixerMode"

#include "mrc_effect_template.h"
#include "webrtc/typedefs.h"

namespace webrtc {
  namespace videocapturemodule {
    private enum class AudioMixerMode
    {
      Mic = 0,
      Loopback = 1,
      MicAndLoopback = 2
    };

    ref class MrcAudioEffectDefinition sealed : public Windows::Media::Effects::IAudioEffectDefinition
    {
    public:
      MrcAudioEffectDefinition();

      //
      // IAudioEffectDefinition
      //
      property Platform::String^ ActivatableClassId
      {
        virtual Platform::String^ get()
        {
          return m_activatableClassId;
        }
      }

      property Windows::Foundation::Collections::IPropertySet^ Properties
      {
        virtual Windows::Foundation::Collections::IPropertySet^ get()
        {
          return m_propertySet;
        }
      }

      //
      // Mixed Reality Capture effect properties
      //
      property AudioMixerMode MixerMode
      {
        AudioMixerMode get()
        {
          return GetValueFromPropertySet<uint32_t>(m_propertySet, PROPERTY_MIXERMODE, DefaultAudioMixerMode);
        }

        void set(AudioMixerMode newValue)
        {
          m_propertySet->Insert(PROPERTY_MIXERMODE, static_cast<uint32_t>(newValue));
        }
      }

    private:
      static constexpr AudioMixerMode DefaultAudioMixerMode = AudioMixerMode::MicAndLoopback;
    private:
      Platform::String^ m_activatableClassId = ref new Platform::String(RUNTIMECLASS_MIXEDREALITYCAPTURE_AUDIO_EFFECT);
      Windows::Foundation::Collections::PropertySet^ m_propertySet = ref new Windows::Foundation::Collections::PropertySet();
    };
  }
}

#endif //WEBRTC_MODULES_VIDEO_CAPTURE_WINDOWS_MRC_AUDIO_EFFECT_DEFINITION_H_
