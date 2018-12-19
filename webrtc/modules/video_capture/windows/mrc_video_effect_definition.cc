#include "mrc_video_effect_definition.h"

namespace webrtc {
  namespace videocapturemodule {
    MrcVideoEffectDefinition::MrcVideoEffectDefinition()
    {
      StreamType = DefaultStreamType;
      HologramCompositionEnabled = DefaultHologramCompositionEnabled;
      RecordingIndicatorEnabled = DefaultRecordingIndicatorEnabled;
      VideoStabilizationEnabled = DefaultVideoStabilizationEnabled;
      VideoStabilizationBufferLength = DefaultVideoStabilizationBufferLength;
      //VideoStabilizationBufferLength = 0;
      GlobalOpacityCoefficient = DefaultGlobalOpacityCoefficient;
    }
  }
}
