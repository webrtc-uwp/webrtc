#ifndef HOLOLIGHT_SIGNALING_SIGNALINGRELAY_H
#define HOLOLIGHT_SIGNALING_SIGNALINGRELAY_H

#include <functional>
#include <string>

namespace hololight::signaling {

class RelayMessage {
 public:
  std::string contentType;
  std::string content;

  RelayMessage(){};

  RelayMessage(std::string contentType, std::string content)
      : contentType(contentType), content(content) {}

  RelayMessage(const RelayMessage& msg)
      : contentType(msg.contentType), content(msg.content) {}
};

class SignalingRelay {
 public:
  using message_handler = std::function<void(RelayMessage&)>;

  virtual ~SignalingRelay(){};

  // Non-blocking, thread-safe
  // Return value only indicates successful SCHEDULING of the send operation
  virtual void send_async(RelayMessage& message) = 0;

  // Non-blocking, (maybe) not thread-safe
  // Will SYNCHRONOUSLY call messageHandler for every message that was received
  // The return value indicates if at least one message was received
  // throws when there is a connection failure
  virtual bool poll_messages() = 0;

  // Blocking, (maybe) not thread-safe
  virtual void close() = 0;

  void register_message_handler(message_handler messageHandler) {
    _messageHandler = messageHandler;
  };

 protected:
  message_handler _messageHandler = nullptr;
};

}  // namespace hololight::signaling

#endif  // HOLOLIGHT_SIGNALING_SIGNALINGRELAY_H