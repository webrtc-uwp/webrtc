#include "webrtc_signaling_relay.h"
#include "../../util/string_utils.h"
#include "rtc_base/logging.h"

#include <sstream>

using namespace hololight::util;
using namespace std::placeholders;

namespace hololight::signaling::webrtc {
WebrtcSignalingRelay::WebrtcSignalingRelay(
    std::unique_ptr<SignalingRelay> relay)
    : _relay(std::move(relay)) {
  _relay->register_message_handler(
      std::bind(&WebrtcSignalingRelay::handle_relay_message, this, _1));
}

void WebrtcSignalingRelay::register_sdp_handler(sdp_handler handler) {
  _onSdp = handler;
}

void WebrtcSignalingRelay::register_ice_candidate_handler(
    ice_candidate_handler handler) {
  _onIceCandidate = handler;
}

bool WebrtcSignalingRelay::poll_messages() {
  return _relay->poll_messages();
}

void WebrtcSignalingRelay::send_sdp_async(Sdp& sdp) {
  RTC_LOG(LS_INFO) << "Sending SDP";

  RelayMessage msg;

  switch (sdp.type) {
    case SdpType::offer:
      msg = RelayMessage("sdp-offer", sdp.content);
      break;

    case SdpType::answer:
      msg = RelayMessage("sdp-answer", sdp.content);
      break;
  }

  _relay->send_async(msg);
}

void WebrtcSignalingRelay::send_ice_candidate_async(
    IceCandidate& iceCandidate) {
  RTC_LOG(LS_INFO) << "Sending ICE candidate";

  std::stringstream msgContent;

  msgContent << iceCandidate.candidate << ',' << iceCandidate.sdp_mid << ','
             << iceCandidate.sdp_mlineindex;

  RelayMessage msg("ice-candidate", msgContent.str());

  _relay->send_async(msg);
}

void WebrtcSignalingRelay::handle_relay_message(RelayMessage& msg) {
  if ("sdp-offer" == msg.contentType) {
    RTC_LOG(LS_INFO) << "Received SDP oofer.. OOF";

    Sdp sdp{SdpType::offer, msg.content};
    _onSdp(sdp);
  } else if ("sdp-answer" == msg.contentType) {
    RTC_LOG(LS_INFO) << "Received SDP answer";

    Sdp sdp{SdpType::answer, msg.content};
    _onSdp(sdp);
  } else if ("ice-candidate" == msg.contentType) {
    RTC_LOG(LS_INFO) << "Received ICE candidate";

    auto msgParts = string_utils::split(msg.content, ',');

    IceCandidate cand{msgParts[0], msgParts[1], std::stoi(msgParts[2])};
    _onIceCandidate(cand);
  } else {
    RTC_LOG(LS_WARNING)
        << "Unknown message type received via WebrtcSignalingRelay";
  }
}
}  // namespace hololight::signaling::webrtc
