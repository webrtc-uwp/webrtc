#ifndef WEBRTC_MODULES_VIDEO_CAPTURE_WINDOWS_MRC_EFFECT_TEMPLATE_H_
#define WEBRTC_MODULES_VIDEO_CAPTURE_WINDOWS_MRC_EFFECT_TEMPLATE_H_

#include <algorithm>

namespace webrtc {
  namespace videocapturemodule {
    template<typename T, typename U>
    U GetValueFromPropertySet(Windows::Foundation::Collections::IPropertySet^ propertySet, Platform::String^ key, U defaultValue)
    {
      try
      {
        return static_cast<U>(safe_cast<T>(propertySet->Lookup(key)));
      }
      catch (Platform::OutOfBoundsException^)
      {
        // The key is not present in the PropertySet. Return the default value.
        return defaultValue;
      }
    }
  }
}

#endif //WEBRTC_MODULES_VIDEO_CAPTURE_WINDOWS_MRC_EFFECT_TEMPLATE_H_
