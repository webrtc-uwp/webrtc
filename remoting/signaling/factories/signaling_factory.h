#ifndef HOLOLIGHT_SIGNALING_FACTORIES_SIGNALINGFACTORY_H
#define HOLOLIGHT_SIGNALING_FACTORIES_SIGNALINGFACTORY_H

#include "../webrtc/webrtc_signaling_relay.h"

namespace hololight::signaling::factories {

class SignalingFactory {
 public:
  std::unique_ptr<hololight::signaling::webrtc::WebrtcSignalingRelay>
  create_tcp_relay_from_connect(std::string ip, unsigned short port);

  std::unique_ptr<hololight::signaling::webrtc::WebrtcSignalingRelay>
  create_tcp_relay_from_listen(unsigned short port);
};
}  // namespace hololight::signaling::factories

#endif  // HOLOLIGHT_SIGNALING_FACTORIES_SIGNALINGFACTORY_H