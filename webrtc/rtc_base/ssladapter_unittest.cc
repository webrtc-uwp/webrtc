/*
 *  Copyright 2014 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <list>
#include <memory>
#include <string>

#include "webrtc/rtc_base/gunit.h"
#include "webrtc/rtc_base/ipaddress.h"
#include "webrtc/rtc_base/socketstream.h"
#include "webrtc/rtc_base/ssladapter.h"
#include "webrtc/rtc_base/sslidentity.h"
#include "webrtc/rtc_base/sslstreamadapter.h"
#include "webrtc/rtc_base/stream.h"
#include "webrtc/rtc_base/stringencode.h"
#include "webrtc/rtc_base/virtualsocketserver.h"

static const int kTimeout = 5000;

static rtc::AsyncSocket* CreateSocket(const rtc::SSLMode& ssl_mode) {
  rtc::SocketAddress address(rtc::IPAddress(INADDR_ANY), 0);

  rtc::AsyncSocket* socket = rtc::Thread::Current()->
      socketserver()->CreateAsyncSocket(
      address.family(), (ssl_mode == rtc::SSL_MODE_DTLS) ?
      SOCK_DGRAM : SOCK_STREAM);
  socket->Bind(address);

  return socket;
}

static std::string GetSSLProtocolName(const rtc::SSLMode& ssl_mode) {
  return (ssl_mode == rtc::SSL_MODE_DTLS) ? "DTLS" : "TLS";
}

class SSLAdapterTestDummyClient : public sigslot::has_slots<> {
 public:
  explicit SSLAdapterTestDummyClient(const rtc::SSLMode& ssl_mode,
                                     rtc::SSLAdapterFactory* factory)
      : ssl_mode_(ssl_mode), ssl_factory_(factory) {
    rtc::AsyncSocket* socket = CreateSocket(ssl_mode_);

    // Use the factory if supplied.
    if (factory) {
      ssl_adapter_.reset(ssl_factory_->CreateAdapter(socket));
    } else {
      ssl_adapter_.reset(rtc::SSLAdapter::Create(socket));
      ssl_adapter_->SetMode(ssl_mode_);
    }

    // Ignore any certificate errors for the purpose of testing.
    // Note: We do this only because we don't have a real certificate.
    // NEVER USE THIS IN PRODUCTION CODE!
    ssl_adapter_->set_ignore_bad_cert(true);

    ssl_adapter_->SignalReadEvent.connect(this,
        &SSLAdapterTestDummyClient::OnSSLAdapterReadEvent);
    ssl_adapter_->SignalCloseEvent.connect(this,
        &SSLAdapterTestDummyClient::OnSSLAdapterCloseEvent);
  }

  rtc::SocketAddress GetAddress() const {
    return ssl_adapter_->GetLocalAddress();
  }

  rtc::AsyncSocket::ConnState GetState() const {
    return ssl_adapter_->GetState();
  }
  bool IsResumedSession() const { return ssl_adapter_->IsResumedSession(); }

  const std::string& GetReceivedData() const {
    return data_;
  }

  int Connect(const std::string& hostname, const rtc::SocketAddress& address) {
    LOG(LS_INFO) << "Initiating connection with " << address;

    int rv = ssl_adapter_->Connect(address);

    if (rv == 0) {
      LOG(LS_INFO) << "Starting " << GetSSLProtocolName(ssl_mode_)
          << " handshake with " << hostname;

      if (ssl_adapter_->StartSSL(hostname.c_str(), false) != 0) {
        return -1;
      }
    }

    return rv;
  }

  int Close() {
    return ssl_adapter_->Close();
  }

  int Send(const std::string& message) {
    LOG(LS_INFO) << "Client sending '" << message << "'";

    return ssl_adapter_->Send(message.data(), message.length());
  }

  void OnSSLAdapterReadEvent(rtc::AsyncSocket* socket) {
    char buffer[4096] = "";

    // Read data received from the server and store it in our internal buffer.
    int read = socket->Recv(buffer, sizeof(buffer) - 1, nullptr);
    if (read != -1) {
      buffer[read] = '\0';

      LOG(LS_INFO) << "Client received '" << buffer << "'";

      data_ += buffer;
    }
  }

  void OnSSLAdapterCloseEvent(rtc::AsyncSocket* socket, int error) {
    // OpenSSLAdapter signals handshake failure with a close event, but without
    // closing the socket! Let's close the socket here. This way GetState() can
    // return CS_CLOSED after failure.
    if (socket->GetState() != rtc::AsyncSocket::CS_CLOSED) {
      socket->Close();
    }
  }

 private:
  const rtc::SSLMode ssl_mode_;
  rtc::SSLAdapterFactory* ssl_factory_;
  std::unique_ptr<rtc::SSLAdapter> ssl_adapter_;

  std::string data_;
};

class SSLAdapterTestDummyServer : public sigslot::has_slots<> {
 public:
  struct Connection {
    explicit Connection(rtc::SSLStreamAdapter* adapter)
        : ssl_adapter(adapter) {}
    std::unique_ptr<rtc::SSLStreamAdapter> ssl_adapter;
    std::string data;
  };

  SSLAdapterTestDummyServer(const rtc::SSLMode& ssl_mode,
                            const rtc::KeyParams& key_params)
      : ssl_mode_(ssl_mode) {
    // Generate a key pair and a certificate for this host.
    auto ssl_identity = rtc::SSLIdentity::Generate(GetHostname(), key_params);
    ssl_factory_.reset(rtc::SSLStreamAdapterFactory::Create());
    ssl_factory_->SetIdentity(ssl_identity);
    ssl_factory_->SetMode(ssl_mode_);
    ssl_factory_->SetRole(rtc::SSL_SERVER);
    // SSLStreamAdapter is normally used for peer-to-peer communication, but
    // here we're testing communication between a client and a server
    // (e.g. a WebRTC-based application and an RFC 5766 TURN server), where
    // clients are not required to provide a certificate during handshake.
    // Accordingly, we must disable client authentication here.
    ssl_factory_->SetClientAuthEnabled(false);
    server_socket_.reset(CreateSocket(ssl_mode_));

    if (ssl_mode_ == rtc::SSL_MODE_TLS) {
      server_socket_->SignalReadEvent.connect(this,
          &SSLAdapterTestDummyServer::OnServerSocketReadEvent);

      server_socket_->Listen(1);
    }

    LOG(LS_INFO) << ((ssl_mode_ == rtc::SSL_MODE_DTLS) ? "UDP" : "TCP")
        << " server listening on " << server_socket_->GetLocalAddress();
  }
  ~SSLAdapterTestDummyServer() {
    while (!ssl_connections_.empty()) {
      RemoveConnection(ssl_connections_.front());
    }
  }

  rtc::SocketAddress GetAddress() const {
    return server_socket_->GetLocalAddress();
  }

  std::string GetHostname() const {
    // Since we don't have a real certificate anyway, the value here doesn't
    // really matter.
    return "example.com";
  }

  Connection* GetLastConnection() {
    if (ssl_connections_.empty()) {
      return nullptr;
    }

    return ssl_connections_.back();
  }

  const std::string& GetReceivedData(Connection* connection) const {
    return connection->data;
  }

  int Send(Connection* connection, const std::string& message) {
    rtc::SSLStreamAdapter* adapter = connection->ssl_adapter.get();
    if (!adapter || adapter->GetState() != rtc::SS_OPEN) {
      // No connection yet.
      return -1;
    }

    LOG(LS_INFO) << "Server sending '" << message << "'";

    size_t written;
    int error;

    rtc::StreamResult r =
        adapter->Write(message.data(), message.length(), &written, &error);
    if (r == rtc::SR_SUCCESS) {
      return written;
    } else {
      return -1;
    }
  }

  void AcceptConnection(const rtc::SocketAddress& address) {
    // Note that multiple connections aren't currently supported for DTLS.
    RTC_DCHECK(server_socket_);
    // This is only for DTLS.
    ASSERT_EQ(rtc::SSL_MODE_DTLS, ssl_mode_);
    // Transfer ownership of the socket to the SSLStreamAdapter object.
    rtc::AsyncSocket* socket = server_socket_.release();
    socket->Connect(address);
    CreateConnection(socket);
  }

  void OnServerSocketReadEvent(rtc::AsyncSocket* socket) {
    // This is only for TLS.
    ASSERT_EQ(rtc::SSL_MODE_TLS, ssl_mode_);
    CreateConnection(server_socket_->Accept(nullptr));
  }

  void OnSSLStreamAdapterEvent(rtc::StreamInterface* stream, int sig, int err) {
    Connection* connection = FindConnection(stream);
    if (sig & rtc::SE_READ) {
      char buffer[4096] = "";
      size_t read;
      int error;

      // Read data received from the client and store it in our internal
      // buffer.
      rtc::StreamResult r =
          stream->Read(buffer, sizeof(buffer) - 1, &read, &error);
      if (r == rtc::SR_SUCCESS) {
        buffer[read] = '\0';
        LOG(LS_INFO) << "Server received '" << buffer << "'";
        connection->data += buffer;
      }
    } else if (sig & rtc::SE_CLOSE) {
      RemoveConnection(connection);
    }
  }

 private:
  Connection* FindConnection(rtc::StreamInterface* stream) {
    for (auto connection : ssl_connections_) {
      if (stream == connection->ssl_adapter.get()) {
        return connection;
      }
    }
    return nullptr;
  }
  // Creates a new s2c SSL connection and starts the SSL handshake.
  Connection* CreateConnection(rtc::AsyncSocket* socket) {
    rtc::SocketStream* stream = new rtc::SocketStream(socket);
    rtc::SSLStreamAdapter* adapter = ssl_factory_->CreateAdapter(stream);
    adapter->StartSSL();
    adapter->SignalEvent.connect(
        this, &SSLAdapterTestDummyServer::OnSSLStreamAdapterEvent);

    Connection* connection = new Connection(adapter);
    ssl_connections_.push_back(connection);
    return connection;
  }
  // Destroys a SSL connection.
  void RemoveConnection(Connection* connection) {
    ssl_connections_.remove(connection);
    delete connection;
  }

  const rtc::SSLMode ssl_mode_;
  std::unique_ptr<rtc::AsyncSocket> server_socket_;
  std::unique_ptr<rtc::SSLStreamAdapterFactory> ssl_factory_;
  std::list<Connection*> ssl_connections_;
};

class SSLAdapterTestBase : public testing::Test,
                           public sigslot::has_slots<> {
 public:
  explicit SSLAdapterTestBase(const rtc::SSLMode& ssl_mode,
                              const rtc::KeyParams& key_params)
      : ssl_mode_(ssl_mode),
        vss_(new rtc::VirtualSocketServer()),
        thread_(vss_.get()),
        handshake_wait_(kTimeout) {
    CreateServer(key_params);
    client_.reset(CreateClient(nullptr));
  }

  void SetHandshakeWait(int wait) {
    handshake_wait_ = wait;
  }

  void TestHandshake(bool expect_success) {
    int rv;

    // The initial state is CS_CLOSED
    ASSERT_EQ(rtc::AsyncSocket::CS_CLOSED, client_->GetState());

    rv = client_->Connect(server_->GetHostname(), server_->GetAddress());
    ASSERT_EQ(0, rv);

    // Now the state should be CS_CONNECTING
    ASSERT_EQ(rtc::AsyncSocket::CS_CONNECTING, client_->GetState());

    if (ssl_mode_ == rtc::SSL_MODE_DTLS) {
      // For DTLS, call AcceptConnection() with the client's address.
      server_->AcceptConnection(client_->GetAddress());
    }

    if (expect_success) {
      // If expecting success, the client should end up in the CS_CONNECTED
      // state after handshake.
      EXPECT_EQ_WAIT(rtc::AsyncSocket::CS_CONNECTED, client_->GetState(),
          handshake_wait_);

      LOG(LS_INFO) << GetSSLProtocolName(ssl_mode_) << " handshake complete.";

    } else {
      // On handshake failure the client should end up in the CS_CLOSED state.
      EXPECT_EQ_WAIT(rtc::AsyncSocket::CS_CLOSED, client_->GetState(),
          handshake_wait_);

      LOG(LS_INFO) << GetSSLProtocolName(ssl_mode_) << " handshake failed.";
    }
  }

  void TestResume() {
    std::unique_ptr<SSLAdapterTestDummyClient> client1;
    std::unique_ptr<SSLAdapterTestDummyClient> client2;
    std::unique_ptr<rtc::SSLAdapterFactory> factory(
        rtc::SSLAdapterFactory::Create());
    factory->SetMode(ssl_mode_);

    // Connect two clients in parallel. Neither one should end up resuming,
    // since we can only resume a session once it has successfuly been
    // established (which requires 2 RTT).
    client1.reset(CreateClient(factory.get()));
    client2.reset(CreateClient(factory.get()));
    ASSERT_EQ(0,
              client1->Connect(server_->GetHostname(), server_->GetAddress()));
    ASSERT_EQ(0,
              client2->Connect(server_->GetHostname(), server_->GetAddress()));
    EXPECT_EQ_WAIT(rtc::AsyncSocket::CS_CONNECTED, client1->GetState(),
                   handshake_wait_);
    EXPECT_EQ_WAIT(rtc::AsyncSocket::CS_CONNECTED, client2->GetState(),
                   handshake_wait_);
    EXPECT_FALSE(client1->IsResumedSession());
    EXPECT_FALSE(client2->IsResumedSession());

    // Again, connect two clients in parallel. Both should end up resuming,
    // since we successfully established a SSL session to the same hostname
    // above.
    client1.reset(CreateClient(factory.get()));
    client2.reset(CreateClient(factory.get()));
    ASSERT_EQ(0,
              client1->Connect(server_->GetHostname(), server_->GetAddress()));
    ASSERT_EQ(0,
              client2->Connect(server_->GetHostname(), server_->GetAddress()));
    EXPECT_EQ_WAIT(rtc::AsyncSocket::CS_CONNECTED, client1->GetState(),
                   handshake_wait_);
    EXPECT_EQ_WAIT(rtc::AsyncSocket::CS_CONNECTED, client2->GetState(),
                   handshake_wait_);
    EXPECT_TRUE(client1->IsResumedSession());
    EXPECT_TRUE(client2->IsResumedSession());

    // Try one more session, but to a new hostname. This should succeed but
    // not resume.
    client1.reset(CreateClient(factory.get()));
    ASSERT_EQ(0, client1->Connect("notexample.com", server_->GetAddress()));
    EXPECT_EQ_WAIT(rtc::AsyncSocket::CS_CONNECTED, client1->GetState(),
                   handshake_wait_);
    EXPECT_FALSE(client1->IsResumedSession());
  }

  void TestTransfer(const std::string& message) {
    auto connection = server_->GetLastConnection();
    int rv;

    rv = client_->Send(message);
    ASSERT_EQ(static_cast<int>(message.length()), rv);

    // The server should have received the client's message.
    EXPECT_EQ_WAIT(message, server_->GetReceivedData(connection), kTimeout);

    rv = server_->Send(connection, message);
    ASSERT_EQ(static_cast<int>(message.length()), rv);

    // The client should have received the server's message.
    EXPECT_EQ_WAIT(message, client_->GetReceivedData(), kTimeout);

    LOG(LS_INFO) << "Transfer complete.";
  }

 protected:
  void CreateServer(const rtc::KeyParams& key_params) {
    server_.reset(new SSLAdapterTestDummyServer(ssl_mode_, key_params));
  }

  SSLAdapterTestDummyClient* CreateClient(rtc::SSLAdapterFactory* factory) {
    return new SSLAdapterTestDummyClient(ssl_mode_, factory);
  }

 protected:
  const rtc::SSLMode ssl_mode_;

  std::unique_ptr<rtc::VirtualSocketServer> vss_;
  rtc::AutoSocketServerThread thread_;
  std::unique_ptr<SSLAdapterTestDummyServer> server_;
  std::unique_ptr<SSLAdapterTestDummyClient> client_;

  int handshake_wait_;
};

class SSLAdapterTestTLS_RSA : public SSLAdapterTestBase {
 public:
  SSLAdapterTestTLS_RSA()
      : SSLAdapterTestBase(rtc::SSL_MODE_TLS, rtc::KeyParams::RSA()) {}
};

class SSLAdapterTestTLS_ECDSA : public SSLAdapterTestBase {
 public:
  SSLAdapterTestTLS_ECDSA()
      : SSLAdapterTestBase(rtc::SSL_MODE_TLS, rtc::KeyParams::ECDSA()) {}
};

class SSLAdapterTestDTLS_RSA : public SSLAdapterTestBase {
 public:
  SSLAdapterTestDTLS_RSA()
      : SSLAdapterTestBase(rtc::SSL_MODE_DTLS, rtc::KeyParams::RSA()) {}
};

class SSLAdapterTestDTLS_ECDSA : public SSLAdapterTestBase {
 public:
  SSLAdapterTestDTLS_ECDSA()
      : SSLAdapterTestBase(rtc::SSL_MODE_DTLS, rtc::KeyParams::ECDSA()) {}
};

// Basic tests: TLS

// Test that handshake works, using RSA
TEST_F(SSLAdapterTestTLS_RSA, TestTLSConnect) {
  TestHandshake(true);
}

// Test that handshake works, using ECDSA
TEST_F(SSLAdapterTestTLS_ECDSA, TestTLSConnect) {
  TestHandshake(true);
}

// Test that a second handshake resumes, using RSA
TEST_F(SSLAdapterTestTLS_RSA, TestTLSResume) {
  TestResume();
}

// Test that a second handshake resumes, using ECDSA
TEST_F(SSLAdapterTestTLS_ECDSA, TestTLSResume) {
  TestResume();
}

// Test transfer between client and server, using RSA
TEST_F(SSLAdapterTestTLS_RSA, TestTLSTransfer) {
  TestHandshake(true);
  TestTransfer("Hello, world!");
}

TEST_F(SSLAdapterTestTLS_RSA, TestTLSTransferWithBlockedSocket) {
  TestHandshake(true);
  auto connection = server_->GetLastConnection();

  // Tell the underlying socket to simulate being blocked.
  vss_->SetSendingBlocked(true);

  std::string expected;
  int rv;
  // Send messages until the SSL socket adapter starts applying backpressure.
  // Note that this may not occur immediately since there may be some amount of
  // intermediate buffering (either in our code or in BoringSSL).
  for (int i = 0; i < 1024; ++i) {
    std::string message = "Hello, world: " + rtc::ToString(i);
    rv = client_->Send(message);
    if (rv != static_cast<int>(message.size())) {
      // This test assumes either the whole message or none of it is sent.
      ASSERT_EQ(-1, rv);
      break;
    }
    expected += message;
  }
  // Assert that the loop above exited due to Send returning -1.
  ASSERT_EQ(-1, rv);

  // Try sending another message while blocked. -1 should be returned again and
  // it shouldn't end up received by the server later.
  EXPECT_EQ(-1, client_->Send("Never sent"));

  // Unblock the underlying socket. All of the buffered messages should be sent
  // without any further action.
  vss_->SetSendingBlocked(false);
  EXPECT_EQ_WAIT(expected, server_->GetReceivedData(connection), kTimeout);

  // Send another message. This previously wasn't working
  std::string final_message = "Fin.";
  expected += final_message;
  EXPECT_EQ(static_cast<int>(final_message.size()),
            client_->Send(final_message));
  EXPECT_EQ_WAIT(expected, server_->GetReceivedData(connection), kTimeout);
}

// Test transfer between client and server, using ECDSA
TEST_F(SSLAdapterTestTLS_ECDSA, TestTLSTransfer) {
  TestHandshake(true);
  TestTransfer("Hello, world!");
}

// Basic tests: DTLS

// Test that handshake works, using RSA
TEST_F(SSLAdapterTestDTLS_RSA, TestDTLSConnect) {
  TestHandshake(true);
}

// Test that handshake works, using ECDSA
TEST_F(SSLAdapterTestDTLS_ECDSA, TestDTLSConnect) {
  TestHandshake(true);
}

// Test transfer between client and server, using RSA
TEST_F(SSLAdapterTestDTLS_RSA, TestDTLSTransfer) {
  TestHandshake(true);
  TestTransfer("Hello, world!");
}

// Test transfer between client and server, using ECDSA
TEST_F(SSLAdapterTestDTLS_ECDSA, TestDTLSTransfer) {
  TestHandshake(true);
  TestTransfer("Hello, world!");
}
