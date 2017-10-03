#ifndef VR_AUDIO_API_VRAUDIO_API_H_
#define VR_AUDIO_API_VRAUDIO_API_H_

#include <cstddef>
#include <string>  // size_t declaration.
#include <vector>

// Avoid dependency to base/integral_types.h
typedef int16_t int16;

namespace vraudio {

// High-level API for VR Audio. All methods of this class are non-blocking and
// thread-safe.
//
// TODO(b/24514226) Add brief introduction and sample usage snippet.
class VrAudioApi {
 public:
  // Sound object / ambisonic source identifier.
  typedef int SourceId;

  // Invalid source id that can be used to initialize handler variables during
  // class construction.
  static const SourceId kInvalidSourceId = -1;

  // Number of octave bands in which reverb is computed.
  static const size_t kNumReverbOctaveBands = 9;

  // Rendering modes define CPU load / rendering quality balances.
  // Note that this struct is C-compatible by design to be used across external
  // C/C++ and C# implementations.
  enum RenderingMode {
    // Stereo panning, i.e., this disables HRTF-based rendering.
    kStereoPanning = 0,
    // HRTF-based rendering using First Order Ambisonics, over a virtual array
    // of 8 loudspeakers arranged in a cube configuration around the listener’s
    // head.
    kBinauralLowQuality,
    // HRTF-based rendering using Second Order Ambisonics, over a virtual array
    // of 12 loudspeakers arranged in a dodecahedral configuration (using faces
    // of the dodecahedron).
    kBinauralMediumQuality,
    // HRTF-based rendering using Third Order Ambisonics, over a virtual array
    // of 26 loudspeakers arranged in a Lebedev grid: https://goo.gl/DX1wh3.
    kBinauralHighQuality,
    // Room effects only rendering. This disables HRTF-based rendering and
    // direct (dry) output of a sound object. Note that this rendering mode
    // should *not* be used for general-purpose sound object spatialization, as
    // it will only render the corresponding room effects of given sound objects
    // without the direct spatialization.
    kRoomEffectsOnly,
  };

  // Distance rolloff models used for distance attenuation.
  // Note that this enum is C-compatible by design to be used across external
  // C/C++ and C# implementations.
  enum DistanceRolloffModel {
    // Logarithmic distance rolloff model.
    kLogarithmic = 0,
    // Linear distance rolloff model.
    kLinear,
    // Distance attenuation value will be explicitly set by the user.
    kNone,
  };

  // Room surface material names, used to set room properties.
  // Note that this enum is C-compatible by design to be used across external
  // C/C++ and C# implementations.
  enum MaterialName {
    kTransparent = 0,
    kAcousticCeilingTiles,
    kBrickBare,
    kBrickPainted,
    kConcreteBlockCoarse,
    kConcreteBlockPainted,
    kCurtainHeavy,
    kFiberGlassInsulation,
    kGlassThin,
    kGlassThick,
    kGrass,
    kLinoleumOnConcrete,
    kMarble,
    kMetal,
    kParquetOnConcrete,
    kPlasterRough,
    kPlasterSmooth,
    kPlywoodPanel,
    kPolishedConcreteOrTile,
    kSheetrock,
    kWaterOrIceSurface,
    kWoodCeiling,
    kWoodPanel,
    kUniform,
    kNumMaterialNames,
  };

  // Acoustic room properties.
  // Note that this struct is C-compatible by design to be used across external
  // C/C++ and C# implementations.
  // LINT.IfChange
  struct RoomProperties {
    RoomProperties()
        : position{0.0f, 0.0f, 0.0f},
          rotation{0.0f, 0.0f, 0.0f, 1.0f},
          dimensions{0.0f, 0.0f, 0.0f},
          material_names{
              MaterialName::kTransparent, MaterialName::kTransparent,
              MaterialName::kTransparent, MaterialName::kTransparent,
              MaterialName::kTransparent, MaterialName::kTransparent},
          reflection_scalar(1.0f),
          reverb_gain(1.0f),
          reverb_time(1.0f),
          reverb_brightness(0.0f) {}

    // Center position of the room in world space.
    float position[3];

    // Rotation (quaternion) of the room in world space.
    float rotation[4];

    // Size of the shoebox room in world space.
    float dimensions[3];

    // Material name of each surface of the shoebox room in this order:
    //     [0]  (-)ive x-axis wall (left)
    //     [1]  (+)ive x-axis wall (right)
    //     [2]  (-)ive y-axis wall (bottom)
    //     [3]  (+)ive y-axis wall (top)
    //     [4]  (-)ive z-axis wall (front)
    //     [5]  (+)ive z-axis wall (back)
    MaterialName material_names[6];

    // User defined uniform scaling factor for all reflection coefficients.
    float reflection_scalar;

    // User defined reverb tail gain multiplier.
    float reverb_gain;

    // Parameter which allows the reverberation time across all frequency bands
    // to be increased or decreased. The calculated RT60 values are multiplied
    // by this factor. This parameter has no effect when set to 1.0f.
    float reverb_time;

    // Parameter which allows the ratio of high frequncy reverb components to
    // low frequency reverb components to be adjusted. This parameter
    // essentially conrols the slope of a line from the lowest reverb frequency
    // to the highest. This parameter has no effect when set to 0.0f.
    float reverb_brightness;
  };
  // LINT.ThenChange(//depot/google3/vr/audio/platform/common/room_properties.h,
  // //depot/google3/third_party/unity/cardboard_plugin/Assets/GoogleVR/Scripts/Audio/GvrAudio.cs,
  // //depot/google3/third_party/unity/vraudio_plugins/fmod/UnityIntegration/Assets/GoogleVR/Scripts/Audio/FmodGvrAudio.cs,
  // //depot/google3/third_party/unity/vraudio_plugins/wwise/UnityIntegration/Assets/GoogleVR/Scripts/Audio/WwiseGvrAudio.cs)

  // Factory method to create a |VrAudioApi| instance. Note that the returned
  // instance must be deleted with |VrAudioApi::Destroy|.
  //
  // @param num_channels Number of channels of audio output.
  // @param frames_per_buffer Number of frames per buffer.
  // @param sample_rate_hz System sample rate.
  static VrAudioApi* Create(size_t num_channels,
                            size_t frames_per_buffer,
                            int sample_rate_hz);

  // Destroys a |VrAudioApi| instance.
  static void Destroy(VrAudioApi** vr_audio_api_ptr);

  // Renders and outputs an interleaved output buffer in float format.
  //
  // @param num_frames Size of output buffer in frames.
  // @param num_channels Number of channels in output buffer.
  // @param buffer_ptr Raw float pointer to audio buffer.
  // @return True if a valid output was successfully rendered, false otherwise.
  virtual bool FillInterleavedOutputBuffer(size_t num_channels,
                                           size_t num_frames,
                                           float* buffer_ptr) = 0;

  // Renders and outputs an interleaved output buffer in int16 format.
  //
  // @param num_channels Number of channels in output buffer.
  // @param num_frames Size of output buffer in frames.
  // @param buffer_ptr Raw int16 pointer to audio buffer.
  // @return True if a valid output was successfully rendered, false otherwise.
  virtual bool FillInterleavedOutputBuffer(size_t num_channels,
                                           size_t num_frames,
                                           int16* buffer_ptr) = 0;

  // Renders and outputs a planar output buffer in float format.
  //
  // @param num_frames Size of output buffer in frames.
  // @param num_channels Number of channels in output buffer.
  // @param buffer_ptr Pointer to array of raw float pointers to each channel of
  //    the audio buffer.
  // @return True if a valid output was successfully rendered, false otherwise.
  virtual bool FillPlanarOutputBuffer(size_t num_channels,
                                      size_t num_frames,
                                      float* const* buffer_ptr) = 0;

  // Renders and outputs a planar output buffer in int16 format.
  //
  // @param num_channels Number of channels in output buffer.
  // @param num_frames Size of output buffer in frames.
  // @param buffer_ptr Pointer to array of raw int16 pointers to each channel of
  //    the audio buffer.
  // @return True if a valid output was successfully rendered, false otherwise.
  virtual bool FillPlanarOutputBuffer(size_t num_channels,
                                      size_t num_frames,
                                      int16* const* buffer_ptr) = 0;

  // Sets listener's head position.
  //
  // @param x X coordinate of head position in world space.
  // @param y Y coordinate of head position in world space.
  // @param z Z coordinate of head position in world space.
  virtual void SetHeadPosition(float x, float y, float z) = 0;

  // Sets listener's head rotation.
  //
  // @param x X component of quaternion.
  // @param y Y component of quaternion.
  // @param z Z component of quaternion.
  // @param w W component of quaternion.
  virtual void SetHeadRotation(float x, float y, float z, float w) = 0;

  // Sets the master volume of the main audio output.
  //
  // @param volume Master volume (linear) in amplitude in range [0, 1] for
  //     attenuation, range [1, inf) for gain boost.
  virtual void SetMasterVolume(float volume) = 0;

  // Enables the stereo speaker mode. When activated, it disables HRTF-based
  // filtering and switches to computationally cheaper stereo-panning. This
  // helps to avoid HRTF-based coloring effects when stereo speakers are used
  // and reduces computational complexity when headphone-based HRTF filtering is
  // not needed. By default the stereo speaker mode is disabled. Note that
  // stereo speaker mode overrides the |enable_hrtf| flag in
  // |CreateSoundObjectSource|.
  //
  // @param enabled Flag to enable stereo speaker mode.
  virtual void SetStereoSpeakerMode(bool enabled) = 0;

  // Creates an ambisonic source instance.
  //
  // @param num_channels Number of input channels.
  // @return Id of new ambisonic source.
  virtual SourceId CreateAmbisonicSource(size_t num_channels) = 0;

  // Creates a stereo non-spatialized source instance, which directly plays back
  // mono or stereo audio.
  //
  // @param num_channels Number of input channels.
  // @return Id of new non-spatialized source.
  virtual SourceId CreateStereoSource(size_t num_channels) = 0;

  // Creates a sound object source instance.
  //
  // @param rendering_mode Rendering mode which governs quality and performance.
  // @return Id of new sound object source.
  virtual SourceId CreateSoundObjectSource(RenderingMode rendering_mode) = 0;

  // Destroys source instance.
  //
  // @param source_id Id of source to be destroyed.
  virtual void DestroySource(SourceId id) = 0;

  // Sets the next audio buffer in interleaved float format to a sound source.
  //
  // @param source_id Id of sound source.
  // @param audio_buffer_ptr Pointer to interleaved float audio buffer.
  // @param num_channels Number of channels in interleaved audio buffer.
  // @param num_frames Number of frames per channel in interleaved audio buffer.
  virtual void SetInterleavedBuffer(SourceId source_id,
                                    const float* audio_buffer_ptr,
                                    size_t num_channels,
                                    size_t num_frames) = 0;

  // Sets the next audio buffer in interleaved int16 format to a sound source.
  //
  // @param source_id Id of sound source.
  // @param audio_buffer_ptr Pointer to interleaved int16 audio buffer.
  // @param num_channels Number of channels in interleaved audio buffer.
  // @param num_frames Number of frames per channel in interleaved audio buffer.
  virtual void SetInterleavedBuffer(SourceId source_id,
                                    const int16* audio_buffer_ptr,
                                    size_t num_channels,
                                    size_t num_frames) = 0;

  // Sets the next audio buffer in planar float format to a sound source.
  //
  // @param source_id Id of sound source.
  // @param audio_buffer_ptr Pointer to array of pointers referring to planar
  //    audio buffers for each channel.
  // @param num_channels Number of planar input audio buffers.
  // @param num_frames Number of frames per channel.
  virtual void SetPlanarBuffer(SourceId source_id,
                               const float* const* audio_buffer_ptr,
                               size_t num_channels,
                               size_t num_frames) = 0;

  // Sets the next audio buffer in planar int16 format to a sound source.
  //
  // @param source_id Id of sound source.
  // @param audio_buffer_ptr Pointer to array of pointers referring to planar
  //    audio buffers for each channel.
  // @param num_channels Number of planar input audio buffers.
  // @param num_frames Number of frames per channel.
  virtual void SetPlanarBuffer(SourceId source_id,
                               const int16* const* audio_buffer_ptr,
                               size_t num_channels,
                               size_t num_frames) = 0;

  // Sets whether the room effects should be bypassed for given source.
  //
  // @param source_id Id of source.
  // @param bypass_room_effects Whether to bypass room effects.
  virtual void SetSourceBypassRoomEffects(SourceId source_id,
                                          bool bypass_room_effects) = 0;

  // Sets the given source's distance attenuation value explicitly. The distance
  // rolloff model of the source must be set to |DistanceRolloffModel::kNone|
  // for the set value to take effect.
  //
  // @param source_id Id of source.
  // @param distance_attenuation Distance attenuation value.
  virtual void SetSourceDistanceAttenuation(SourceId source_id,
                                            float distance_attenuation) = 0;

  // Sets the given source's distance attenuation method with minimum and
  // maximum distances. Maximum distance must be greater than the minimum
  // distance for the method to be set.
  //
  // @param source_id Id of source.
  // @param rolloff Linear or logarithmic distance rolloff models.
  // @param min_distance Minimum distance to apply distance attenuation method.
  // @param max_distance Maximum distance to apply distance attenuation method.
  virtual void SetSourceDistanceModel(SourceId source_id,
                                      DistanceRolloffModel rolloff,
                                      float min_distance,
                                      float max_distance) = 0;

  // Sets the given source's position. Note that, the given position for an
  // ambisonic source is only used to determine the corresponding room effects
  // to be applied.
  // TODO(b/34892710): Use ambisonic source positions for playback management.
  //
  // @param source_id Id of source.
  // @param x X coordinate of source position in world space.
  // @param y Y coordinate of source position in world space.
  // @param z Z coordinate of source position in world space.
  virtual void SetSourcePosition(SourceId source_id,
                                 float x,
                                 float y,
                                 float z) = 0;

  // Sets the given source's rotation.
  //
  // @param source_id Id of source.
  // @param x X component of quaternion.
  // @param y Y component of quaternion.
  // @param z Z component of quaternion.
  // @param w W component of quaternion.
  virtual void SetSourceRotation(SourceId source_id,
                                 float x,
                                 float y,
                                 float z,
                                 float w) = 0;

  // Sets the given source's volume.
  //
  // @param source_id Id of source.
  // @param volume Source volume (linear) in amplitude in range [0, 1] for
  //     attenuation, range [1, inf) for gain boost.
  virtual void SetSourceVolume(SourceId source_id, float volume) = 0;

  // Sets the given sound object source's directivity.
  //
  // @param sound_object_source_id Id of sound object source.
  // @param alpha Weighting balance between figure of eight pattern and circular
  //     pattern for source emission in range [0, 1]. A value of 0.5 results in
  //     a cardioid pattern.
  // @param order Order applied to computed directivity. Higher values will
  //     result in narrower and sharper directivity patterns. Range [1, inf).
  virtual void SetSoundObjectDirectivity(SourceId sound_object_source_id,
                                         float alpha,
                                         float order) = 0;

  // Sets the listener's directivity with respect to the given sound object.
  // This method could be used to simulate an angular rolloff in terms of the
  // listener's orientation, given the polar pickup pattern with |alpha| and
  // |order|.
  //
  // @param sound_object_source_id Id of sound object source.
  // @param alpha Weighting balance between figure of eight pattern and circular
  //     pattern for listener's pickup in range [0, 1]. A value of 0.5 results
  //     in a cardioid pattern.
  // @param order Order applied to computed pickup pattern. Higher values will
  //     result in narrower and sharper pickup patterns. Range [1, inf).
  virtual void SetSoundObjectListenerDirectivity(
      SourceId sound_object_source_id,
      float alpha,
      float order) = 0;

  // Sets the given sound object source's occlusion intensity.
  //
  // @param sound_object_source_id Id of sound object source.
  // @param intensity Number of occlusions occurred for the object. The value
  //     can be set to fractional for partial occlusions. Range [0, inf).
  virtual void SetSoundObjectOcclusionIntensity(SourceId sound_object_source_id,
                                                float intensity) = 0;

  // Sets the given sound object source's spread.
  //
  // @param sound_object_source_id Id of sound object source.
  // @param spread_deg Spread in degrees.
  virtual void SetSoundObjectSpread(SourceId sound_object_source_id,
                                    float spread_deg) = 0;

  // Turns on/off the reflections and reverberation.
  virtual void EnableRoomEffects(bool enable) = 0;

  // Sets the room properties for the reflections and/or reverberation.
  //
  // TODO(b/36065815): Replace this with direct setting of reflection and
  // reverb properties, with computation from room properties performed
  // elsewhere.
  //
  // @param room_properties Room properties.
  virtual void SetRoomProperties(const RoomProperties& room_properties) = 0;

 protected:
  // Protected destructor to prevent user deletion.
  virtual ~VrAudioApi() {}
};

}  // namespace vraudio

#endif  // VR_AUDIO_API_VRAUDIO_API_H_
