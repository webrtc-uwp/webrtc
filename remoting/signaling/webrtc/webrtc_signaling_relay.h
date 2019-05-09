#ifndef HOLOLIGHT_SIGNALING_WEBRTC_WEBRTCSIGNALINGRELAY_H
#define HOLOLIGHT_SIGNALING_WEBRTC_WEBRTCSIGNALINGRELAY_H

#include "../signaling_relay.h"

namespace hololight::signaling::webrtc {

enum SdpType { offer, answer };

struct Sdp {
  SdpType type;
  std::string content;
};

struct IceCandidate {
  std::string candidate;
  std::string sdp_mid;
  int sdp_mlineindex;
};

// TODO: Think about how this thing should handle invalid messages
class WebrtcSignalingRelay {
  using sdp_handler = std::function<void(Sdp&)>;
  using ice_candidate_handler = std::function<void(IceCandidate&)>;

 public:
  WebrtcSignalingRelay(std::unique_ptr<SignalingRelay> relay);

  void register_sdp_handler(sdp_handler handler);
  void register_ice_candidate_handler(ice_candidate_handler handler);

  // Will invoke handlers on the calling thread if messages were received.
  // NOT guaranteed to be thread-safe
  // return value indicates if at least one message was received
  bool poll_messages();

  void send_sdp_async(Sdp& sdp);

  void send_ice_candidate_async(IceCandidate& iceCandidate);

 private:
  void handle_relay_message(RelayMessage& msg);

  std::unique_ptr<SignalingRelay> _relay;
  sdp_handler _onSdp;
  ice_candidate_handler _onIceCandidate;
};
}  // namespace hololight::signaling::webrtc

#endif  // HOLOLIGHT_SIGNALING_WEBRTC_WEBRTCSIGNALINGRELAY_H