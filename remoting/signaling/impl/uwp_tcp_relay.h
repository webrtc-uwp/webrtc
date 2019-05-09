#ifndef HOLOLIGHT_SIGNALING_IMPL_UWPTCPRELAY_H
#define HOLOLIGHT_SIGNALING_IMPL_UWPTCPRELAY_H

#include "../serializer.h"
#include "../signaling_relay.h"

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/base.h>

#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.System.h>

#include <winrt/Windows.Networking.Sockets.h>
#include <winrt/Windows.Networking.h>

#include <future>
#include <mutex>
#include <optional>
#include <queue>

namespace hololight::signaling::impl {

class UwpTcpRelay : public SignalingRelay {
 public:
  UwpTcpRelay(winrt::Windows::Networking::Sockets::StreamSocket streamSocket,
              std::unique_ptr<Serializer> serializer);
  ~UwpTcpRelay();

  static std::unique_ptr<UwpTcpRelay> create_from_connect(std::string ip,
                                                          unsigned short port);
  static std::unique_ptr<UwpTcpRelay> create_from_listen(unsigned short port);

  // Implementation of SignalingRelay
  void send_async(RelayMessage& message) override;
  bool poll_messages() override;
  void close() override;

 private:
  bool should_terminate(std::shared_future<void>& exitSignal);

  std::optional<RelayMessage> wait_for_message_to_send(
      std::shared_future<void>& exitSignal);
  void send_message(hololight::signaling::RelayMessage& msg,
                    winrt::Windows::Storage::Streams::DataWriter& writer);

  // Returns the number of bytes that were received IN EXCESS of the requested
  // number of bytes
  void receive_at_least(
      unsigned int bytesToRead,
      winrt::Windows::Storage::Streams::DataReader const& reader);

  std::promise<void> _exitSignal;

  std::thread _sendThread;
  void send_loop(std::shared_future<void> exitSignal);
  std::mutex _sendLock;
  std::queue<RelayMessage> _sendQueue;

  std::thread _recvThread;
  void recv_loop(std::shared_future<void> exitSignal);
  std::mutex _recvLock;
  std::queue<RelayMessage> _recvQueue;

  std::unique_ptr<Serializer> _serializer;

  winrt::Windows::Networking::Sockets::StreamSocket _streamSocket;
};

}  // namespace hololight::signaling::impl

#endif  // HOLOLIGHT_SIGNALING_IMPL_UWPTCPRELAY_H