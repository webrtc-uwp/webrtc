#include "uwp_tcp_relay.h"

using std::unique_ptr;
using std::string;
using std::promise;
using std::lock_guard;
using std::mutex;
using std::optional;

#include <thread>
#include "../../util/future_utils.h"
#include "../../util/string_utils.h"
#include "json_serializer.h"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Storage::Streams;
using namespace winrt::Windows::Networking;
using namespace winrt::Windows::Networking::Sockets;

using namespace hololight::util;

namespace hololight::signaling::impl {

unique_ptr<UwpTcpRelay> UwpTcpRelay::create_from_connect(
    string ip,
    unsigned short port) {
  StreamSocket ss;

  HostName host{string_utils::string_to_wstring(ip)};

  ss.ConnectAsync(host, std::to_wstring(port)).get();

  auto serializer = std::make_unique<JsonSerializer>();

  auto relay = std::make_unique<UwpTcpRelay>(ss, std::move(serializer));

  return relay;
}

unique_ptr<UwpTcpRelay> UwpTcpRelay::create_from_listen(
    unsigned short port) {
  StreamSocketListener listener;

  promise<unique_ptr<UwpTcpRelay>> promise;

  auto future = promise.get_future();

  auto task = [&](StreamSocketListener sender,
                  StreamSocketListenerConnectionReceivedEventArgs args) {
    auto serializer = std::make_unique<JsonSerializer>();

    auto relay =
        std::make_unique<UwpTcpRelay>(args.Socket(), std::move(serializer));

    promise.set_value(std::move(relay));
  };

  listener.ConnectionReceived(std::move(task));

  listener.BindServiceNameAsync(std::to_wstring(port)).get();

  return future.get();
}

UwpTcpRelay::UwpTcpRelay(StreamSocket streamSocket,
                             unique_ptr<Serializer> serializer)
    : _streamSocket(streamSocket),
      _serializer(std::move(serializer)),
      _exitSignal() {
  std::shared_future<void> exit_future(_exitSignal.get_future());

  _sendThread = std::thread(&UwpTcpRelay::send_loop, this, exit_future);
  _recvThread = std::thread(&UwpTcpRelay::recv_loop, this, exit_future);
}

UwpTcpRelay::~UwpTcpRelay() {
  close();  // TODO: Make sure this is only called once!
}

void UwpTcpRelay::send_async(RelayMessage& message) {
  lock_guard<mutex> guard(_sendLock);

  _sendQueue.push(message);  // TODO: Somehow throw when sending WILL fail
}

bool UwpTcpRelay::poll_messages() {
  // TODO: Think about if this copy is smart or if we should lock longer, but
  // don't copy

  std::queue<RelayMessage> messages;

  {
    lock_guard<mutex> guard(_recvLock);

    messages = _recvQueue;
    std::queue<RelayMessage>().swap(_recvQueue);
  }

  if (messages.empty()) {
    return false;
  }

  do {
    auto msg = messages.front();

    messages.pop();

    _messageHandler(msg);

  } while (!messages.empty());

  return true;
}

void UwpTcpRelay::close() {
  _streamSocket.Close();  // TODO: Also called when delete is called... Should
                          // we call this explicitly? And when?

  // TODO: Think about if we should do the stuff below BEFORE closing the socket

  _exitSignal.set_value();

  _sendThread.join();
  _recvThread.join();
}

inline bool UwpTcpRelay::should_terminate(
    std::shared_future<void>& exitSignal) {
  return future_utils::is_ready(exitSignal);
}

void UwpTcpRelay::send_loop(std::shared_future<void> exitSignal) {
  try {
    DataWriter writer{_streamSocket.OutputStream()};

    while (!should_terminate(exitSignal)) {
      auto msg = wait_for_message_to_send(exitSignal);

      if (!msg.has_value()) {
        break;
      }

      send_message(msg.value(), writer);
    }

    writer.DetachStream();
  } catch (const std::exception&) {
    // TODO: Proper error handling
  } catch (winrt::hresult_error const&) {
    // TODO: Proper error handling
  }
}

optional<RelayMessage> UwpTcpRelay::wait_for_message_to_send(
    std::shared_future<void>& exitSignal) {
  optional<RelayMessage> msg = std::nullopt;

  do {
    {
      lock_guard<mutex> guard(_sendLock);

      if (!_sendQueue.empty()) {
        msg = _sendQueue.front();

        _sendQueue.pop();

        break;
      }
    }

    std::this_thread::yield();

  } while (!should_terminate(exitSignal));

  return msg;
}

void UwpTcpRelay::send_message(
    RelayMessage& msg,
    winrt::Windows::Storage::Streams::DataWriter& writer) {
  // TODO: Think about cancellation here if we must

  auto serialized_utf8 = _serializer->serialize(msg);

  winrt::hstring serialized_wide{
      hololight::util::string_utils::string_to_wstring(serialized_utf8)};

  writer.WriteUInt32(serialized_wide.size());
  writer.WriteString(serialized_wide);

  writer.StoreAsync().get();
}

void UwpTcpRelay::receive_at_least(
    unsigned int bytesToRead,
    winrt::Windows::Storage::Streams::DataReader const& reader) {
  // TODO: Think about cancellation here if we must

  while (reader.UnconsumedBufferLength() < bytesToRead) {
    reader.LoadAsync(bytesToRead - reader.UnconsumedBufferLength()).get();
  }
}

void UwpTcpRelay::recv_loop(std::shared_future<void> exitSignal) {
  try {
    DataReader reader{_streamSocket.InputStream()};

    while (!should_terminate(exitSignal)) {
      receive_at_least(sizeof(unsigned int), reader);

      unsigned int msgLength = reader.ReadUInt32();

      receive_at_least(msgLength, reader);

      auto enc_message_wide_h = reader.ReadString(msgLength);

      // TODO: See if we can make that method also accept hstring directly
      std::wstring enc_message_wide{enc_message_wide_h};
      auto enc_message_utf8 =
          hololight::util::string_utils::wstring_to_string(enc_message_wide);

      auto message = _serializer->deserialize(enc_message_utf8);

      {
        lock_guard<mutex> guard(_recvLock);

        _recvQueue.push(message);
      }
    }

    reader.DetachStream();  // TODO: Figure out if necessary
  } catch (const std::exception&) {
    // TODO: Proper error handling
  } catch (winrt::hresult_error const&) {
    // TODO: Proper error handling
  }
}
}  // namespace hololight::signaling::impl
