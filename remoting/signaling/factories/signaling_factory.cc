#include "signaling_factory.h"

#include "../impl/uwp_tcp_relay.h"

using namespace hololight::signaling::impl;
using hololight::signaling::webrtc::WebrtcSignalingRelay;

// TODO: This class should also choose the correct relay implementation for each
// platform
namespace hololight::signaling::factories {

std::unique_ptr<WebrtcSignalingRelay>
SignalingFactory::create_tcp_relay_from_connect(std::string ip,
                                                unsigned short port) {
  return std::make_unique<WebrtcSignalingRelay>(
      UwpTcpRelay::create_from_connect(ip, port));
}

std::unique_ptr<WebrtcSignalingRelay>
SignalingFactory::create_tcp_relay_from_listen(unsigned short port) {
  return std::make_unique<WebrtcSignalingRelay>(
      UwpTcpRelay::create_from_listen(port));
}
}  // namespace hololight::signaling::factories
