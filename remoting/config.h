#ifndef HOLOLIGHT_REMOTING_CONFIG_H
#define HOLOLIGHT_REMOTING_CONFIG_H

#include <vector>
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"

// so, what belongs into a config?
// role: {client, server}
// video source: {None, D3D, Webcam, ...}
// encoder: {None, Builtin (all supported codecs), UWP_H264}
// decoder: {None, Builtin (same as above), UWP_H264 (if ms makes one)}
// ice_servers: {stun:stun.l.google.com:19302, ...}
// signaling: ip (when client), port (always)

// example:
// {
//   "role": "client",
//   "video-source": "none",
//   "encoder": "none",
//   "decoder": "h264-uwp",
//   "ice-servers": [
//       "stun:stun.l.google.com:19302",
//       "..."
//   ],
//   "signaling": {
//       "ip": "192.168.0.1",
//       "port": 36500
//   }
// }

//ice-servers should be optional because I think we don't need them when doing things locally.
//signaling should be mandatory, but depends on the signaling method, i.e. tcp relay needs both
//on client, but only port on server or such. or we call them differently, i.e. "tcp-signaling"
//or "websocket-signaling" and provide all the info inside.
// Markus: We could change IP to something like "destination" and decide the kind of signalling
//         based on the protocol prefix: E.g. No prefix: TCP, wss:// prefix: Websockets
//         But this kind of hides that the WebSocket implementation does A LOT of stuff differently
//Also, encoder/decoder "none" option could get thrown out because markus mentioned shit breaking
//down the line when there are none because (I think) sdp needs it or something like that.

class Config {
 public:
 
  static Config from_file(absl::string_view file_path);

  static Config default_server() {
    Config c;
    c.role = Role::Server;
    c.video_source = VideoSource::D3D11;
    c.encoder = Encoder::H264_UWP;
    c.decoder = Decoder::None;
    return c;
  };

  // we don't want to create a d3d device for testing, so no video source.
  static Config default_server_test() {
    Config c;
    c.role = Role::Server;
    c.video_source = VideoSource::None;
    c.encoder = Encoder::H264_UWP;
    c.decoder = Decoder::None;
    c.signaling_port = "36500";
    return c;
  };

  static Config default_client() {
    Config c;
    c.role = Role::Client;
    c.video_source = VideoSource::None;
    c.encoder = Encoder::None;
    c.decoder = Decoder::H264_UWP;
    c.signaling_ip = "192.168.0.1";  // insert something meaningful here
    c.signaling_port = "36500";
    return c;
  }

  // is this really necessary? I guess for creating the tracks, at least on the
  // server side... but then again, we do have videosource already which
  // influences the created tracks.
  enum class Role { Server, Client };

  enum class VideoSource { None, D3D11, Webcam };

  enum class Encoder {
    None,     // not sure if this is even supported when creating
              // peerconnectionfactory
    Builtin,  // default ones (SW)
    H264_UWP  // windows 10 only...but what about other platforms then? what do
              // they do if the variant is unavailable?
    // just throw a not supported error and bail out if we get a garbage config
  };

  enum class Decoder {
    None,  // server needs no decoder
    Builtin,
    H264_UWP
  };

  Role role;
  VideoSource video_source;
  Encoder encoder;
  Decoder decoder;
  std::vector<std::string> ice_servers;

  // these are for connecting to the signaling server. if we're using tcp relay
  // for debugging, signaling_ip/port is the IP to connect to (from client
  // side)/ port to listen on (on server side).
  absl::optional<std::string> signaling_ip;
  absl::optional<std::string> signaling_port;
};

// Markus:
// Alternative: Server and client don't necessarily care about the same stuff, so we might
// want to split it up into two classes
// We can also expose these configs through a C API so users can create them in their preferred way

#endif  // HOLOLIGHT_REMOTING_CONFIG_H