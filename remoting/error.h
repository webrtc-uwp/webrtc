#ifndef HOLOLIGHT_REMOTING_ERROR_H
#define HOLOLIGHT_REMOTING_ERROR_H

namespace hololight::remoting {
// should this be (or have some kind of conversion to) an outward-facing error
// type? Markus: Yes.
class Error {
 public:
  enum class Type {
    None,
    AlreadyInitialized,     // Init has already been called
    PeerConnectionFactory,  // CreatePeerConnectionFactory failed
    PeerConnection,         // CreatePeerConnection failed
    DataChannel,            // CreateDataChannel failed
    UnsupportedConfig,      // config contains values not supported on platform,
                            // i.e. d3d11 on android
    AddTrack,               // AddTrack failed
    VideoSource,
    StartRtcEventLog,       // StartRtcEventLog failed
    // TODO: fill in things that can go wrong and we want the user to know.
  };

  Error(Type t) : type(t) {}

  Type type;
};
}  // namespace hololight::remoting

#endif  // HOLOLIGHT_REMOTING_ERROR_H