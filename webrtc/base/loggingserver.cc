#include "webrtc/base/loggingserver.h"
#include "webrtc/base/socketaddress.h"
#include "webrtc/base/thread.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/platform_thread.h"
#include "webrtc/base/physicalsocketserver.h"

namespace rtc {

LoggingServer::LoggingServer() {
  PhysicalSocketServer* pss = new PhysicalSocketServer();
  thread_ = new Thread(pss);
}

LoggingServer::~LoggingServer() {
  if (listener_)
    listener_->Close();

  for (auto it = connections_.begin();
               it != connections_.end(); ++it) {
    LogMessage::RemoveLogToStream(it->second);
    it->second->Detach();
    // Post messages to delete doomed objects
    thread_->Dispose(it->first);
    thread_->Dispose(it->second);
  }
  connections_.clear();
  thread_->Stop();
  tw_->Stop();
}

int LoggingServer::Listen(const SocketAddress& addr, LoggingSeverity level) {
  level_ = level;
  tw_.reset(new PlatformThread(&LoggingServer::processMessages, thread_, "LoggingServer"));
  tw_->Start();

  LOG(LS_INFO) << "New LoggingServer thread created.";

  AsyncSocket* sock =
    thread_->socketserver()->CreateAsyncSocket(AF_INET, SOCK_STREAM);

  if (!sock) {
    return SOCKET_ERROR;
  }

  listener_.reset(sock);
  listener_->SignalReadEvent.connect(this, &LoggingServer::OnAcceptEvent);

  // Bind to the specified address and listen incoming connections.
  // Maximum 5 pending connections are allowed.
  if ((listener_->Bind(addr) != SOCKET_ERROR) &&
    (listener_->Listen(5) != SOCKET_ERROR)) {

    // Send wake up signal to update the event list to wait
    thread_->socketserver()->WakeUp();

    return 0;
  }

  return listener_->GetError();
}

void LoggingServer::OnAcceptEvent(AsyncSocket* socket) {
  if (!listener_)
    return;

  if (socket != listener_.get())
    return;

  AsyncSocket* incoming = listener_->Accept(NULL);

  if (incoming) {
    // Attach the socket of accepted connection to a stream.
    LogSinkImpl* stream = new LogSinkImpl(incoming);
    connections_.push_back(std::make_pair(incoming, stream));

    // Add new non-blocking stream to log messages.
    LogMessage::AddLogToStream(stream, level_);
    incoming->SignalCloseEvent.connect(this, &LoggingServer::OnCloseEvent);

    LOG(LS_INFO) << "Successfully connected to the logging server!";
  }
}

void LoggingServer::OnCloseEvent(AsyncSocket* socket, int err) {
  if (!socket)
    return;

  LOG(LS_INFO) << "Connection closed : " << err;

  for (auto it = connections_.begin();
               it != connections_.end(); ++it) {
    if (socket == it->first) {
      LogMessage::RemoveLogToStream(it->second);
      it->second->Detach();

      // Post messages to delete doomed objects
      thread_->Dispose(it->first);
      thread_->Dispose(it->second);
      it = connections_.erase(it);
      break;
    }
  }
}

bool LoggingServer::processMessages(void* args) {
  Thread* t = reinterpret_cast<Thread*>(args);
  t->Run();
  return true;
}

}  // namespace rtc
