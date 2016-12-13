
// Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef WEBRTC_BUILD_WINRT_GYP_API_MEDIA_H_
#define WEBRTC_BUILD_WINRT_GYP_API_MEDIA_H_

#include <mfidl.h>
#include <collection.h>
#include "webrtc/api/peerconnectioninterface.h"
#include "webrtc/api/mediastreaminterface.h"
#include "webrtc/api/mediaconstraintsinterface.h"
#include "webrtc/base/scoped_ptr.h"
#include "GlobalObserver.h"
#include "webrtc/media/devices/devicemanager.h"
#include "webrtc/media/devices/winrtdevicemanager.h"
#include "webrtc/modules/audio_device/include/audio_device.h"
#include "Delegates.h"

using Windows::Foundation::IAsyncOperation;
using Platform::String;
using Windows::Foundation::Collections::IVector;
using Windows::Media::Core::IMediaSource;
using Windows::Devices::Enumeration::DeviceWatcher;
using Windows::Devices::Enumeration::DeviceInformation;
using Windows::Devices::Enumeration::DeviceInformationUpdate;

namespace Org {
	namespace WebRtc {

		/// <summary>
		/// An IMediaStreamTrack object represents media of a single type that
		/// originates from one media source, e.g. video produced by a web camera.
		/// </summary>
		/// <remarks>
		/// http://www.w3.org/TR/mediacapture-streams
		/// </remarks>
		public interface class IMediaStreamTrack {
			/// <summary>
			/// Gets a description of the type of media, e.g., "audio" or "video".
			/// </summary>
			property String^ Kind { String^ get(); }

			/// <summary>
			/// Gets an identifier of the media track.
			/// </summary>
			property String^ Id { String^ get(); }

			/// <summary>
			/// Get and sets the availibility of the media.
			/// </summary>
			property bool Enabled { bool get(); void set(bool value); }

			/// <summary>
			/// Stop the media track.
			/// </summary>
			void Stop();
		};

		/// <summary>
		/// Represents video media that originates from one video source.
		/// </summary>
		/// <seealso cref="IMediaStreamTrack"/>
		public ref class MediaVideoTrack sealed : public IMediaStreamTrack {
		internal:
			MediaVideoTrack(rtc::scoped_refptr<webrtc::VideoTrackInterface> impl);
			rtc::scoped_refptr<webrtc::VideoTrackInterface> GetImpl() {
				return _impl;
			}
		public:
			virtual ~MediaVideoTrack();
			/// <summary>
			/// Gets a description of the type of media, e.g., "audio" or "video".
			/// </summary>
			virtual property String^ Kind { String^ get(); }
			/// <summary>
			/// Gets an identifier of the media track.
			/// </summary>
			virtual property String^ Id { String^ get(); }
			/// <summary>
			/// Get and sets the availibility of the media.
			/// </summary>
			virtual property bool Enabled { bool get(); void set(bool value); }
			/// <summary>
			/// Determines or set whether the media track is paused.
			/// </summary>
			property bool Suspended { bool get(); void set(bool value); }
			/// <summary>
			/// Stops the media and releases associated resources.
			/// </summary>
			virtual void Stop();
		internal:
			void SetRenderer(webrtc::VideoRendererInterface* renderer);
			void UnsetRenderer(webrtc::VideoRendererInterface* renderer);
		private:
			rtc::scoped_refptr<webrtc::VideoTrackInterface> _impl;
		};

		/// <summary>
		/// Represents audio media that originates from one audio source.
		/// </summary>
		/// <seealso cref="IMediaStreamTrack"/>
		public ref class MediaAudioTrack sealed : public IMediaStreamTrack {
		internal:
			MediaAudioTrack(rtc::scoped_refptr<webrtc::AudioTrackInterface> impl);
			rtc::scoped_refptr<webrtc::AudioTrackInterface> GetImpl() {
				return _impl;
			}
		public:
			/// <summary>
			/// Gets a description of the type of media, e.g., "audio" or "video".
			/// </summary>
			virtual property String^ Kind { String^ get(); }
			/// <summary>
			/// Gets an identifier of the media track.
			/// </summary>
			virtual property String^ Id { String^ get(); }
			/// <summary>
			/// Get and sets the availibility of the media.
			/// </summary>
			virtual property bool Enabled { bool get(); void set(bool value); }
			/// <summary>
			/// Stops the media and releases associated resources.
			/// </summary>
			virtual void Stop();
		private:
			rtc::scoped_refptr<webrtc::AudioTrackInterface> _impl;
		};

		/// <summary>
		/// A MediaStream is used to group several <see cref="IMediaStreamTrack"/>
		/// objects into one unit that can be recorded or rendered in a media
		/// element. Each MediaStream can contain zero or more
		///  <see cref="IMediaStreamTrack"/> objects.
		/// </summary>
		/// <remarks>
		/// http://www.w3.org/TR/mediacapture-streams/
		/// </remarks>
		public ref class MediaStream sealed {
		internal:

			/// <summary>
			/// Composes a new stream.
			/// </summary>
			/// <param name="impl"></param>
			MediaStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> impl);

			rtc::scoped_refptr<webrtc::MediaStreamInterface> GetImpl();
		public:
			/// <summary>
			/// Returns a Vector that represents a snapshot of all the
			/// <see cref="IMediaStreamTrack"/> objects
			/// in this stream's track set whose kind is equal to "audio".
			/// </summary>
			/// <returns>A Vector of <see cref="IMediaStreamTrack"/>
			/// objects representing the audio tracks in this stream</returns>
			IVector<MediaAudioTrack^>^ GetAudioTracks();

			/// <summary>
			/// Returns a Vector that represents a snapshot of all the
			/// <see cref="IMediaStreamTrack"/> objects
			/// in this stream's track set whose kind is equal to "video".
			/// </summary>
			/// <returns>A Vector of <see cref="IMediaStreamTrack"/> objects
			/// representing the video tracks in this stream</returns>
			IVector<MediaVideoTrack^>^ GetVideoTracks();

			/// <summary>
			/// Returns a Vector that represents a snapshot of all the
			/// <see cref="IMediaStreamTrack"/> objects
			/// in this stream's track set, regardless of kind.
			/// </summary>
			/// <returns>A Vector of <see cref="IMediaStreamTrack"/> objects
			/// representing all the tracks in this stream.</returns>
			/// <seealso cref="GetAudioTracks"/>
			/// <seealso cref="GetVideoTracks"/>
			IVector<IMediaStreamTrack^>^ GetTracks();

			/// <summary>
			/// Return either a <see cref="IMediaStreamTrack"/> object from
			/// this stream's track set whose id is
			/// equal to trackId, or nullptr, if no such track exists.
			/// </summary>
			/// <param name="trackId">The identifier of the track to return</param>
			/// <returns>A <see cref="IMediaStreamTrack"/> object from this stream's
			/// track set whose id is
			/// equal to trackId, or nullptr, if no such track exists.</returns>
			IMediaStreamTrack^ GetTrackById(String^ trackId);

			/// <summary>
			/// Adds the given <see cref="IMediaStreamTrack"/> to this
			/// <see cref="MediaStream"/>.
			/// </summary>
			/// <param name="track">Track to be added to the
			/// <see cref="MediaStream"/></param>
			void AddTrack(IMediaStreamTrack^ track);

			/// <summary>
			/// Removes the given <see cref="IMediaStreamTrack"/> from this
			/// <see cref="MediaStream"/>.
			/// </summary>
			/// <param name="track">Track to be removed from the
			/// <see cref="MediaStream"/></param>
			void RemoveTrack(IMediaStreamTrack^ track);

			/// <summary>
			/// Gets an identifier of the media stream.
			/// </summary>
			property String^ Id { String^ get(); }

			/// <summary>
			/// Stops and releases resources of all tracks within this stream.
			/// </summary>
			void Stop();

			/// <summary>
			/// This attribute is true if the <see cref="MediaStream"/> has at least
			/// one <see cref="IMediaStreamTrack"/>
			/// that has not ended, and false otherwise.
			/// </summary>
			property bool Active { bool get(); }
		private:
			~MediaStream();
			rtc::scoped_refptr<webrtc::MediaStreamInterface> _impl;
		};

		/// <summary>
		/// Represents video camera capture capabilities.
		/// </summary>
		public ref class CaptureCapability sealed {
		public:
			/// <summary>
			/// CaptureCapability constructor.
			/// </summary>
			/// <param name="width">Sets the width in pixels of a capability on the video capture device.</param>
			/// <param name="height">Sets the height in pixesl of a capability on the video capture device.</param>
			/// <param name="fps">Set the frames per second of a capability on the video capture device.</param>
			/// <param name="pixelAspect">Sets the shape of a pixel. Some codecs, such as H.264, support
			/// non-square pixels. Codecs that support only square pixes, such as VPx, will force a 1:1 ratio.</param>
			CaptureCapability(unsigned int width, unsigned int height,
				unsigned int fps,
				Windows::Media::MediaProperties::MediaRatio^ pixelAspect) {
				_width = width;
				_height = height;
				_fps = fps;
				_pixelAspectRatio = pixelAspect;
				wchar_t resolutionDesc[64];
				swprintf_s(resolutionDesc, 64, L"%u x %u",
					_width, _height);
				_resolutionDescription = ref new String(resolutionDesc);
				wchar_t fpsDesc[64];
				swprintf_s(fpsDesc, 64, L"%u fps", _fps);
				_fpsDescription = ref new String(fpsDesc);
				wchar_t desc[128];
				swprintf_s(desc, 128, L"%s %s", resolutionDesc, fpsDesc);
				_description = ref new String(desc);
			}
			/// <summary>
			/// Gets the width in pixes of a video capture device capibility.
			/// </summary>
			property unsigned int Width {
				unsigned int get() {
					return _width;
				}
			}
			/// <summary>
			/// Gets the height in pixes of a video capture device capibility.
			/// </summary>
			property unsigned int Height {
				unsigned int get() {
					return _height;
				}
			}
			/// <summary>
			/// Gets the frame rate in frames per second of a video capture device capibility.
			/// </summary>
			property unsigned int FrameRate {
				unsigned int get() {
					return _fps;
				}
			}
			/// <summary>
			/// Get the aspect ratio of the pixels of a video capture device capibility.
			/// </summary>
			property Windows::Media::MediaProperties::MediaRatio^ PixelAspectRatio {
				Windows::Media::MediaProperties::MediaRatio^ get() {
					return _pixelAspectRatio;
				}
			}
			/// <summary>
			/// Get a displayable string describing all the features of a
			/// video capture device capability. Displays resolution, frame rate,
			/// and pixel aspect ratio.
			/// </summary>
			property String^ FullDescription {
				String^ get() {
					return _description;
				}
			}
			/// <summary>
			/// Get a displayable string describing the resolution of a
			/// video capture device capability.
			/// </summary>
			property String^ ResolutionDescription {
				String^ get() {
					return _resolutionDescription;
				}
			}
			/// <summary>
			/// Get a displayable string describing the frame rate in
			// frames per second of a video capture device capability.
			/// </summary>
			property String^ FrameRateDescription {
				String^ get() {
					return _fpsDescription;
				}
			}
		private:
			unsigned int _width;
			unsigned int _height;
			unsigned int _fps;
			Windows::Media::MediaProperties::MediaRatio^ _pixelAspectRatio;
			String^ _resolutionDescription;
			String^ _fpsDescription;
			String^ _description;
		};

		/// <summary>
		/// Represents a local media device, such as a microphone or a camera.
		/// </summary>
		public ref class MediaDevice sealed {
		private:
			String^ _id;
			String^ _name;
			Windows::Devices::Enumeration::EnclosureLocation^ _location;
		public:
			MediaDevice(String^ id, String^ name) {
				_id = id;
				_name = name;
				_location = nullptr;
			}
			MediaDevice(String^ id, String^ name, Windows::Devices::Enumeration::EnclosureLocation^ location) {
				_id = id;
				_name = name;
				_location = location;
			}
			/// <summary>
			/// Gets or sets an identifier of the media device.
			/// This value defaults to a unique OS assigned identifier of the media device.
			/// </summary>
			property String^ Id {
				String^ get() {
					return _id;
				}
				void set(String^ value) {
					_id = value;
				}
			}

			/// <summary>
			/// Get or sets a displayable name that describes the media device.
			/// </summary>
			property String^ Name {
				String^ get() {
					return _name;
				}
				void set(String^ value) {
					_name = value;
				}
			}

			/// <summary>
			/// Get the location of the media device.
			/// </summary>
			property Windows::Devices::Enumeration::EnclosureLocation^ Location {
				Windows::Devices::Enumeration::EnclosureLocation^ get() {
					return _location;
				}
			}

			/// <summary>
			/// Retrieves video capabilities for a given device.
			/// </summary>
			/// <returns>This is an asynchronous method. The result is a vector of the
			/// capabilities supported by the video device.</returns>
			IAsyncOperation<IVector<CaptureCapability^>^>^
				GetVideoCaptureCapabilities();
		};

		/// <summary>
		/// Allows defining constraints to exclude media types from a
		/// <see cref="MediaStream"/>.
		/// </summary>
		public ref class RTCMediaStreamConstraints sealed {
		public:
			/// <summary>
			/// Set or gets the availability of audio.
			/// </summary>
			property bool audioEnabled;
			/// <summary>
			/// Sets or gets the availability of video.
			/// </summary>
			property bool videoEnabled;
		};

		/// <summary>
		/// Defines methods for accessing local media devices, like microphones
		/// and video cameras, and creating multimedia streams.
		/// </summary>
		/// <remarks>
		/// http://www.w3.org/TR/mediacapture-streams
		/// </remarks>
		public ref class Media sealed {
		public:
			static Media^ CreateMedia();

			virtual ~Media();

			/// <summary>
			/// In order for this method to complete successfully, the user must have
			/// allowed the application permissions to use the devices for the
			/// requested media types (microphone for audio, webcam for video).
			/// Creates a <see cref="MediaStream"/> with both audio and video tracks,
			/// unless the <paramref name="mediaStreamConstraints"/>
			/// is set to exclude either media type.
			/// </summary>
			/// <param name="mediaStreamConstraints">Controls whether video/audio
			/// tracks are included.</param>
			/// <returns>
			/// This is an asynchronous method. The result upon completion is a
			/// <see cref="MediaStream"/> including
			/// audio and/or video tracks, as requested by the
			/// <paramref name="mediaStreamConstraints"/> parameter.
			/// </returns>

			IAsyncOperation<MediaStream^>^ GetUserMedia(
				RTCMediaStreamConstraints^ mediaStreamConstraints);

			/// <summary>
			/// Creates an <see cref="IMediaSource"/> for a video track, with a given
			/// identifier to be used for notifications on media changes.
			/// </summary>
			/// <param name="track">Video track to create a <see cref="IMediaSource"/>
			/// from</param>
			/// <param name="id">Identifier that can be used by applications for
			/// distinguishing between <see cref="MediaStream"/>s
			/// when receiving media change event notifications.
			/// </param>
			/// <returns>A media source.</returns>
			IMediaSource^ CreateMediaStreamSource(
				MediaVideoTrack^ track, uint32 framerate, String^ id);

			/// <summary>
			/// Creates an <see cref="IMediaSource"/> for a video track, with a given
			/// frame rate and identifier to be used for notifications on media
			/// changes.
			/// </summary>
			/// <param name="track">Video track to create a <see cref="IMediaSource"/>
			/// from</param>
			/// <param name="framerate">Target frame rate</param>
			/// <param name="id">Identifier that can be used by applications for
			/// distinguishing between <see cref="MediaStream"/>s
			/// when receiving media change event notifications.
			/// </param>
			/// <returns>A media source.</returns>
			IMediaSource^ CreateMediaSource(
				MediaVideoTrack^ track, String^ id);

			/// <summary>
			/// Retrieves system devices that can be used for audio capturing.
			/// (microphones).
			/// </summary>
			/// <returns>Vector of system devices that can be used for audio capturing
			/// (microphones).</returns>
			IVector<MediaDevice^>^ GetAudioCaptureDevices();

			/// <summary>
			/// Retrieves system devices that can be used for audio playout (speakers).
			/// </summary>
			/// <returns>Vector of system devices that can be used for audio playout
			/// (speakers).</returns>
			IVector<MediaDevice^>^ GetAudioPlayoutDevices();

			/// <summary>
			/// Retrieves system devices that can be used for video capturing (webcams).
			/// </summary>
			/// <returns>Vector of system devices that can be used for video capturing
			/// (webcams).</returns>
			IVector<MediaDevice^>^ GetVideoCaptureDevices();

			/// <summary>
			/// Allows switching between webcams.
			/// </summary>
			/// <param name="device">Webcam to be used for video capturing.</param>
			void SelectVideoDevice(MediaDevice^ device);

			/// <summary>
			/// Allows switching between microphones.
			/// </summary>
			/// <param name="device">Microphone to be used for audio capturing.If null,
			/// default audio recording device will be used</param>
			/// <returns>True if the operation succeeded.</returns>
			bool SelectAudioCaptureDevice(MediaDevice^ device);

			/// <summary>
			/// Allows switching between audio playout devices (speakers).
			/// </summary>
			/// <param name="device">Device to be used for audio playback. If null,
			/// default audio playout device will be used.</param>
			/// <returns>True if the operation succeeded.</returns>
			bool SelectAudioPlayoutDevice(MediaDevice^ device);

			/// <summary>
			/// App suspending event handler.
			/// </summary>
			static void OnAppSuspending();

			/// <summary>
			/// Set display orientation, used to rotate captured video in case the
			/// capturer is attached to the enclosure.
			/// This method should be called only if WebRTC::Initialize was called with
			/// null core dispatcher, otherwise, the display orientation detection is
			/// performed internally.
			/// </summary>
			static void SetDisplayOrientation(Windows::Graphics::Display::DisplayOrientations
				display_orientation);

			/// <summary>
			/// Fired when audio or video device configuration changed.
			/// </summary>
			event MediaDevicesChanged^ OnMediaDevicesChanged;

		private:
			Media();

			void SubscribeToMediaDeviceChanges();
			void UnsubscribeFromMediaDeviceChanges();

			void OnMediaDeviceAdded(DeviceWatcher^ sender,
				DeviceInformation^ args);
			void OnMediaDeviceRemoved(DeviceWatcher^ sender,
				DeviceInformationUpdate^ args);
			static int GetAudioPlayoutDeviceIndex(webrtc::VoEHardware* voeHardware,
				const std::string& name,
				const std::string& id);
			static int GetAudioCaptureDeviceIndex(webrtc::VoEHardware* voeHardware,
				const std::string& name,
				const std::string& id);

			rtc::scoped_ptr<cricket::DeviceManagerInterface> _dev_manager;
			cricket::Device _selectedVideoDevice;
			cricket::Device _selectedAudioCapturerDevice;
			cricket::Device _selectedAudioPlayoutDevice;

			DeviceWatcher^ _videoCaptureWatcher;
			DeviceWatcher^ _audioCaptureWatcher;
			DeviceWatcher^ _audioPlayoutWatcher;
			bool _videoCaptureDeviceChanged;
			bool _audioCaptureDeviceChanged;
			bool _audioPlayoutDeviceChanged;
		};
	}
}  // namespace Org.WebRtc

#endif  // WEBRTC_BUILD_WINRT_GYP_API_MEDIA_H_
