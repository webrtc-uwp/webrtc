## This project has been deprecated

We are currently focusing our efforts on getting out of the fork business. This effort is happening in the [WinRTC GitHub repo](https://github.com/microsoft/winrtc). Keeping WebRTC-UWP fork doesn't allow us to move fast enough. We are contributing back the changes needed to build WebRTC.org code base for UWP. Here are some of the changes we're contributing back:

- https://webrtc-review.googlesource.com/c/src/+/167021
- https://boringssl-review.googlesource.com/c/boringssl/+/39584
- https://chromium-review.googlesource.com/c/chromium/src/+/1962509
- [abseil/abseil-cpp#594](https://github.com/abseil/abseil-cpp/pull/594)
- [abseil/abseil-cpp#596](https://github.com/abseil/abseil-cpp/pull/596)

Besides the new video capturing module that is being reviewed, we're also creating a new audio capturing module. There are more changes in the pipeline to be contributed back and more changes required for finishing the port. After having WebRTC.org code base compatible with UWP, we're going to work on a WinRT abstraction layer allowing easy consumption of WebRTC capabilities by WinRT projections.

That said, keep in mind we are contributing back the changes and we have no control over when/if the changes will be accepted by their teams.

**WebRTC is a free, open software project** that provides browsers and mobile
applications with Real-Time Communications (RTC) capabilities via simple APIs.
The WebRTC components have been optimized to best serve this purpose.

**Our mission:** To enable rich, high-quality RTC applications to be
developed for the browser, mobile platforms, and IoT devices, and allow them
all to communicate via a common set of protocols.

The WebRTC initiative is a project supported by Google, Mozilla and Opera,
amongst others.

### Development

See http://www.webrtc.org/native-code/development for instructions on how to get
started developing with the native code.

[Authoritative list](native-api.md) of directories that contain the
native API header files.

### More info

 * Official web site: http://www.webrtc.org
 * Master source code repo: https://webrtc.googlesource.com/src
 * Samples and reference apps: https://github.com/webrtc
 * Mailing list: http://groups.google.com/group/discuss-webrtc
 * Continuous build: http://build.chromium.org/p/client.webrtc
 * [Coding style guide](style-guide.md)
 * [Code of conduct](CODE_OF_CONDUCT.md)
