/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "webrtc/p2p/base/basicpacketsocketfactory.h"
#include "webrtc/p2p/base/relayport.h"
#include "webrtc/p2p/base/stunport.h"
#include "webrtc/p2p/base/tcpport.h"
#include "webrtc/p2p/base/testrelayserver.h"
#include "webrtc/p2p/base/teststunserver.h"
#include "webrtc/p2p/base/testturnserver.h"
#include "webrtc/p2p/base/transport.h"
#include "webrtc/p2p/base/turnport.h"
#include "webrtc/base/arraysize.h"
#include "webrtc/base/buffer.h"
#include "webrtc/base/crc32.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/helpers.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/natserver.h"
#include "webrtc/base/natsocketfactory.h"
#include "webrtc/base/physicalsocketserver.h"
#include "webrtc/base/socketaddress.h"
#include "webrtc/base/ssladapter.h"
#include "webrtc/base/stringutils.h"
#include "webrtc/base/thread.h"
#include "webrtc/base/virtualsocketserver.h"

using rtc::AsyncPacketSocket;
using rtc::Buffer;
using rtc::ByteBufferReader;
using rtc::ByteBufferWriter;
using rtc::NATType;
using rtc::NAT_OPEN_CONE;
using rtc::NAT_ADDR_RESTRICTED;
using rtc::NAT_PORT_RESTRICTED;
using rtc::NAT_SYMMETRIC;
using rtc::PacketSocketFactory;
using rtc::Socket;
using rtc::SocketAddress;
using namespace cricket;

static const int kTimeout = 1000;
static const SocketAddress kLocalAddr1("192.168.1.2", 0);
static const SocketAddress kLocalAddr2("192.168.1.3", 0);
static const SocketAddress kNatAddr1("77.77.77.77", rtc::NAT_SERVER_UDP_PORT);
static const SocketAddress kNatAddr2("88.88.88.88", rtc::NAT_SERVER_UDP_PORT);
static const SocketAddress kStunAddr("99.99.99.1", STUN_SERVER_PORT);
static const SocketAddress kRelayUdpIntAddr("99.99.99.2", 5000);
static const SocketAddress kRelayUdpExtAddr("99.99.99.3", 5001);
static const SocketAddress kRelayTcpIntAddr("99.99.99.2", 5002);
static const SocketAddress kRelayTcpExtAddr("99.99.99.3", 5003);
static const SocketAddress kRelaySslTcpIntAddr("99.99.99.2", 5004);
static const SocketAddress kRelaySslTcpExtAddr("99.99.99.3", 5005);
static const SocketAddress kTurnUdpIntAddr("99.99.99.4", STUN_SERVER_PORT);
static const SocketAddress kTurnTcpIntAddr("99.99.99.4", 5010);
static const SocketAddress kTurnUdpExtAddr("99.99.99.5", 0);
static const RelayCredentials kRelayCredentials("test", "test");

// TODO: Update these when RFC5245 is completely supported.
// Magic value of 30 is from RFC3484, for IPv4 addresses.
static const uint32_t kDefaultPrflxPriority =
    ICE_TYPE_PREFERENCE_PRFLX << 24 | 30 << 8 |
    (256 - ICE_CANDIDATE_COMPONENT_DEFAULT);

static const int kTiebreaker1 = 11111;
static const int kTiebreaker2 = 22222;

static const char* data = "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";

static Candidate GetCandidate(Port* port) {
  assert(port->Candidates().size() >= 1);
  return port->Candidates()[0];
}

static SocketAddress GetAddress(Port* port) {
  return GetCandidate(port).address();
}

static IceMessage* CopyStunMessage(const IceMessage* src) {
  IceMessage* dst = new IceMessage();
  ByteBufferWriter buf;
  src->Write(&buf);
  ByteBufferReader read_buf(buf);
  dst->Read(&read_buf);
  return dst;
}

static bool WriteStunMessage(const StunMessage* msg, ByteBufferWriter* buf) {
  buf->Resize(0);  // clear out any existing buffer contents
  return msg->Write(buf);
}

// Stub port class for testing STUN generation and processing.
class TestPort : public Port {
 public:
  TestPort(rtc::Thread* thread,
           const std::string& type,
           rtc::PacketSocketFactory* factory,
           rtc::Network* network,
           const rtc::IPAddress& ip,
           uint16_t min_port,
           uint16_t max_port,
           const std::string& username_fragment,
           const std::string& password)
      : Port(thread,
             type,
             factory,
             network,
             ip,
             min_port,
             max_port,
             username_fragment,
             password) {}
  ~TestPort() {}

  // Expose GetStunMessage so that we can test it.
  using cricket::Port::GetStunMessage;

  // The last StunMessage that was sent on this Port.
  // TODO: Make these const; requires changes to SendXXXXResponse.
  Buffer* last_stun_buf() { return last_stun_buf_.get(); }
  IceMessage* last_stun_msg() { return last_stun_msg_.get(); }
  int last_stun_error_code() {
    int code = 0;
    if (last_stun_msg_) {
      const StunErrorCodeAttribute* error_attr = last_stun_msg_->GetErrorCode();
      if (error_attr) {
        code = error_attr->code();
      }
    }
    return code;
  }

  virtual void PrepareAddress() {
    rtc::SocketAddress addr(ip(), min_port());
    AddAddress(addr, addr, rtc::SocketAddress(), "udp", "", "", Type(),
               ICE_TYPE_PREFERENCE_HOST, 0, true);
  }

  virtual bool SupportsProtocol(const std::string& protocol) const {
    return true;
  }

  // Exposed for testing candidate building.
  void AddCandidateAddress(const rtc::SocketAddress& addr) {
    AddAddress(addr, addr, rtc::SocketAddress(), "udp", "", "", Type(),
               type_preference_, 0, false);
  }
  void AddCandidateAddress(const rtc::SocketAddress& addr,
                           const rtc::SocketAddress& base_address,
                           const std::string& type,
                           int type_preference,
                           bool final) {
    AddAddress(addr, base_address, rtc::SocketAddress(), "udp", "", "", type,
               type_preference, 0, final);
  }

  virtual Connection* CreateConnection(const Candidate& remote_candidate,
                                       CandidateOrigin origin) {
    Connection* conn = new ProxyConnection(this, 0, remote_candidate);
    AddConnection(conn);
    // Set use-candidate attribute flag as this will add USE-CANDIDATE attribute
    // in STUN binding requests.
    conn->set_use_candidate_attr(true);
    return conn;
  }
  virtual int SendTo(
      const void* data, size_t size, const rtc::SocketAddress& addr,
      const rtc::PacketOptions& options, bool payload) {
    if (!payload) {
      IceMessage* msg = new IceMessage;
      Buffer* buf = new Buffer(static_cast<const char*>(data), size);
      ByteBufferReader read_buf(*buf);
      if (!msg->Read(&read_buf)) {
        delete msg;
        delete buf;
        return -1;
      }
      last_stun_buf_.reset(buf);
      last_stun_msg_.reset(msg);
    }
    return static_cast<int>(size);
  }
  virtual int SetOption(rtc::Socket::Option opt, int value) {
    return 0;
  }
  virtual int GetOption(rtc::Socket::Option opt, int* value) {
    return -1;
  }
  virtual int GetError() {
    return 0;
  }
  void Reset() {
    last_stun_buf_.reset();
    last_stun_msg_.reset();
  }
  void set_type_preference(int type_preference) {
    type_preference_ = type_preference;
  }

 private:
  void OnSentPacket(rtc::AsyncPacketSocket* socket,
                    const rtc::SentPacket& sent_packet) {
    PortInterface::SignalSentPacket(sent_packet);
  }
  std::unique_ptr<Buffer> last_stun_buf_;
  std::unique_ptr<IceMessage> last_stun_msg_;
  int type_preference_ = 0;
};

class TestChannel : public sigslot::has_slots<> {
 public:
  // Takes ownership of |p1| (but not |p2|).
  TestChannel(Port* p1)
      : ice_mode_(ICEMODE_FULL),
        port_(p1),
        complete_count_(0),
        conn_(NULL),
        remote_request_(),
        nominated_(false) {
    port_->SignalPortComplete.connect(this, &TestChannel::OnPortComplete);
    port_->SignalUnknownAddress.connect(this, &TestChannel::OnUnknownAddress);
    port_->SignalDestroyed.connect(this, &TestChannel::OnSrcPortDestroyed);
  }

  int complete_count() { return complete_count_; }
  Connection* conn() { return conn_; }
  const SocketAddress& remote_address() { return remote_address_; }
  const std::string remote_fragment() { return remote_frag_; }

  void Start() { port_->PrepareAddress(); }
  void CreateConnection(const Candidate& remote_candidate) {
    conn_ = port_->CreateConnection(remote_candidate, Port::ORIGIN_MESSAGE);
    IceMode remote_ice_mode =
        (ice_mode_ == ICEMODE_FULL) ? ICEMODE_LITE : ICEMODE_FULL;
    conn_->set_remote_ice_mode(remote_ice_mode);
    conn_->set_use_candidate_attr(remote_ice_mode == ICEMODE_FULL);
    conn_->SignalStateChange.connect(
        this, &TestChannel::OnConnectionStateChange);
    conn_->SignalDestroyed.connect(this, &TestChannel::OnDestroyed);
    conn_->SignalReadyToSend.connect(this,
                                     &TestChannel::OnConnectionReadyToSend);
    connection_ready_to_send_ = false;
  }
  void OnConnectionStateChange(Connection* conn) {
    if (conn->write_state() == Connection::STATE_WRITABLE) {
      conn->set_use_candidate_attr(true);
      nominated_ = true;
    }
  }
  void AcceptConnection(const Candidate& remote_candidate) {
    ASSERT_TRUE(remote_request_.get() != NULL);
    Candidate c = remote_candidate;
    c.set_address(remote_address_);
    conn_ = port_->CreateConnection(c, Port::ORIGIN_MESSAGE);
    conn_->SignalDestroyed.connect(this, &TestChannel::OnDestroyed);
    port_->SendBindingResponse(remote_request_.get(), remote_address_);
    remote_request_.reset();
  }
  void Ping() {
    Ping(0);
  }
  void Ping(int64_t now) { conn_->Ping(now); }
  void Stop() {
    if (conn_) {
      conn_->Destroy();
    }
  }

  void OnPortComplete(Port* port) {
    complete_count_++;
  }
  void SetIceMode(IceMode ice_mode) {
    ice_mode_ = ice_mode;
  }

  int SendData(const char* data, size_t len) {
    rtc::PacketOptions options;
    return conn_->Send(data, len, options);
  }

  void OnUnknownAddress(PortInterface* port, const SocketAddress& addr,
                        ProtocolType proto,
                        IceMessage* msg, const std::string& rf,
                        bool /*port_muxed*/) {
    ASSERT_EQ(port_.get(), port);
    if (!remote_address_.IsNil()) {
      ASSERT_EQ(remote_address_, addr);
    }
    const cricket::StunUInt32Attribute* priority_attr =
        msg->GetUInt32(STUN_ATTR_PRIORITY);
    const cricket::StunByteStringAttribute* mi_attr =
        msg->GetByteString(STUN_ATTR_MESSAGE_INTEGRITY);
    const cricket::StunUInt32Attribute* fingerprint_attr =
        msg->GetUInt32(STUN_ATTR_FINGERPRINT);
    EXPECT_TRUE(priority_attr != NULL);
    EXPECT_TRUE(mi_attr != NULL);
    EXPECT_TRUE(fingerprint_attr != NULL);
    remote_address_ = addr;
    remote_request_.reset(CopyStunMessage(msg));
    remote_frag_ = rf;
  }

  void OnDestroyed(Connection* conn) {
    ASSERT_EQ(conn_, conn);
    LOG(INFO) << "OnDestroy connection " << conn << " deleted";
    conn_ = NULL;
    // When the connection is destroyed, also clear these fields so future
    // connections are possible.
    remote_request_.reset();
    remote_address_.Clear();
  }

  void OnSrcPortDestroyed(PortInterface* port) {
    Port* destroyed_src = port_.release();
    ASSERT_EQ(destroyed_src, port);
  }

  Port* port() { return port_.get(); }

  bool nominated() const { return nominated_; }

  void set_connection_ready_to_send(bool ready) {
    connection_ready_to_send_ = ready;
  }
  bool connection_ready_to_send() const {
    return connection_ready_to_send_;
  }

 private:
  // ReadyToSend will only issue after a Connection recovers from EWOULDBLOCK.
  void OnConnectionReadyToSend(Connection* conn) {
    ASSERT_EQ(conn, conn_);
    connection_ready_to_send_ = true;
  }

  IceMode ice_mode_;
  std::unique_ptr<Port> port_;

  int complete_count_;
  Connection* conn_;
  SocketAddress remote_address_;
  std::unique_ptr<StunMessage> remote_request_;
  std::string remote_frag_;
  bool nominated_;
  bool connection_ready_to_send_ = false;
};

class PortTest : public testing::Test, public sigslot::has_slots<> {
 public:
  PortTest()
      : main_(rtc::Thread::Current()),
        pss_(new rtc::PhysicalSocketServer),
        ss_(new rtc::VirtualSocketServer(pss_.get())),
        ss_scope_(ss_.get()),
        network_("unittest", "unittest", rtc::IPAddress(INADDR_ANY), 32),
        socket_factory_(rtc::Thread::Current()),
        nat_factory1_(ss_.get(), kNatAddr1, SocketAddress()),
        nat_factory2_(ss_.get(), kNatAddr2, SocketAddress()),
        nat_socket_factory1_(&nat_factory1_),
        nat_socket_factory2_(&nat_factory2_),
        stun_server_(TestStunServer::Create(main_, kStunAddr)),
        turn_server_(main_, kTurnUdpIntAddr, kTurnUdpExtAddr),
        relay_server_(main_,
                      kRelayUdpIntAddr,
                      kRelayUdpExtAddr,
                      kRelayTcpIntAddr,
                      kRelayTcpExtAddr,
                      kRelaySslTcpIntAddr,
                      kRelaySslTcpExtAddr),
        username_(rtc::CreateRandomString(ICE_UFRAG_LENGTH)),
        password_(rtc::CreateRandomString(ICE_PWD_LENGTH)),
        role_conflict_(false),
        destroyed_(false) {
    network_.AddIP(rtc::IPAddress(INADDR_ANY));
  }

 protected:
  void TestLocalToLocal() {
    Port* port1 = CreateUdpPort(kLocalAddr1);
    port1->SetIceRole(cricket::ICEROLE_CONTROLLING);
    Port* port2 = CreateUdpPort(kLocalAddr2);
    port2->SetIceRole(cricket::ICEROLE_CONTROLLED);
    TestConnectivity("udp", port1, "udp", port2, true, true, true, true);
  }
  void TestLocalToStun(NATType ntype) {
    Port* port1 = CreateUdpPort(kLocalAddr1);
    port1->SetIceRole(cricket::ICEROLE_CONTROLLING);
    nat_server2_.reset(CreateNatServer(kNatAddr2, ntype));
    Port* port2 = CreateStunPort(kLocalAddr2, &nat_socket_factory2_);
    port2->SetIceRole(cricket::ICEROLE_CONTROLLED);
    TestConnectivity("udp", port1, StunName(ntype), port2,
                     ntype == NAT_OPEN_CONE, true,
                     ntype != NAT_SYMMETRIC, true);
  }
  void TestLocalToRelay(RelayType rtype, ProtocolType proto) {
    Port* port1 = CreateUdpPort(kLocalAddr1);
    port1->SetIceRole(cricket::ICEROLE_CONTROLLING);
    Port* port2 = CreateRelayPort(kLocalAddr2, rtype, proto, PROTO_UDP);
    port2->SetIceRole(cricket::ICEROLE_CONTROLLED);
    TestConnectivity("udp", port1, RelayName(rtype, proto), port2,
                     rtype == RELAY_GTURN, true, true, true);
  }
  void TestStunToLocal(NATType ntype) {
    nat_server1_.reset(CreateNatServer(kNatAddr1, ntype));
    Port* port1 = CreateStunPort(kLocalAddr1, &nat_socket_factory1_);
    port1->SetIceRole(cricket::ICEROLE_CONTROLLING);
    Port* port2 = CreateUdpPort(kLocalAddr2);
    port2->SetIceRole(cricket::ICEROLE_CONTROLLED);
    TestConnectivity(StunName(ntype), port1, "udp", port2,
                     true, ntype != NAT_SYMMETRIC, true, true);
  }
  void TestStunToStun(NATType ntype1, NATType ntype2) {
    nat_server1_.reset(CreateNatServer(kNatAddr1, ntype1));
    Port* port1 = CreateStunPort(kLocalAddr1, &nat_socket_factory1_);
    port1->SetIceRole(cricket::ICEROLE_CONTROLLING);
    nat_server2_.reset(CreateNatServer(kNatAddr2, ntype2));
    Port* port2 = CreateStunPort(kLocalAddr2, &nat_socket_factory2_);
    port2->SetIceRole(cricket::ICEROLE_CONTROLLED);
    TestConnectivity(StunName(ntype1), port1, StunName(ntype2), port2,
                     ntype2 == NAT_OPEN_CONE,
                     ntype1 != NAT_SYMMETRIC, ntype2 != NAT_SYMMETRIC,
                     ntype1 + ntype2 < (NAT_PORT_RESTRICTED + NAT_SYMMETRIC));
  }
  void TestStunToRelay(NATType ntype, RelayType rtype, ProtocolType proto) {
    nat_server1_.reset(CreateNatServer(kNatAddr1, ntype));
    Port* port1 = CreateStunPort(kLocalAddr1, &nat_socket_factory1_);
    port1->SetIceRole(cricket::ICEROLE_CONTROLLING);
    Port* port2 = CreateRelayPort(kLocalAddr2, rtype, proto, PROTO_UDP);
    port2->SetIceRole(cricket::ICEROLE_CONTROLLED);
    TestConnectivity(StunName(ntype), port1, RelayName(rtype, proto), port2,
                     rtype == RELAY_GTURN, ntype != NAT_SYMMETRIC, true, true);
  }
  void TestTcpToTcp() {
    Port* port1 = CreateTcpPort(kLocalAddr1);
    port1->SetIceRole(cricket::ICEROLE_CONTROLLING);
    Port* port2 = CreateTcpPort(kLocalAddr2);
    port2->SetIceRole(cricket::ICEROLE_CONTROLLED);
    TestConnectivity("tcp", port1, "tcp", port2, true, false, true, true);
  }
  void TestTcpToRelay(RelayType rtype, ProtocolType proto) {
    Port* port1 = CreateTcpPort(kLocalAddr1);
    port1->SetIceRole(cricket::ICEROLE_CONTROLLING);
    Port* port2 = CreateRelayPort(kLocalAddr2, rtype, proto, PROTO_TCP);
    port2->SetIceRole(cricket::ICEROLE_CONTROLLED);
    TestConnectivity("tcp", port1, RelayName(rtype, proto), port2,
                     rtype == RELAY_GTURN, false, true, true);
  }
  void TestSslTcpToRelay(RelayType rtype, ProtocolType proto) {
    Port* port1 = CreateTcpPort(kLocalAddr1);
    port1->SetIceRole(cricket::ICEROLE_CONTROLLING);
    Port* port2 = CreateRelayPort(kLocalAddr2, rtype, proto, PROTO_SSLTCP);
    port2->SetIceRole(cricket::ICEROLE_CONTROLLED);
    TestConnectivity("ssltcp", port1, RelayName(rtype, proto), port2,
                     rtype == RELAY_GTURN, false, true, true);
  }
  // helpers for above functions
  UDPPort* CreateUdpPort(const SocketAddress& addr) {
    return CreateUdpPort(addr, &socket_factory_);
  }
  UDPPort* CreateUdpPort(const SocketAddress& addr,
                         PacketSocketFactory* socket_factory) {
    return UDPPort::Create(main_, socket_factory, &network_, addr.ipaddr(), 0,
                           0, username_, password_, std::string(), true);
  }
  TCPPort* CreateTcpPort(const SocketAddress& addr) {
    return CreateTcpPort(addr, &socket_factory_);
  }
  TCPPort* CreateTcpPort(const SocketAddress& addr,
                        PacketSocketFactory* socket_factory) {
    return TCPPort::Create(main_, socket_factory, &network_,
                           addr.ipaddr(), 0, 0, username_, password_,
                           true);
  }
  StunPort* CreateStunPort(const SocketAddress& addr,
                           rtc::PacketSocketFactory* factory) {
    ServerAddresses stun_servers;
    stun_servers.insert(kStunAddr);
    return StunPort::Create(main_, factory, &network_,
                            addr.ipaddr(), 0, 0,
                            username_, password_, stun_servers,
                            std::string());
  }
  Port* CreateRelayPort(const SocketAddress& addr, RelayType rtype,
                        ProtocolType int_proto, ProtocolType ext_proto) {
    if (rtype == RELAY_TURN) {
      return CreateTurnPort(addr, &socket_factory_, int_proto, ext_proto);
    } else {
      return CreateGturnPort(addr, int_proto, ext_proto);
    }
  }
  TurnPort* CreateTurnPort(const SocketAddress& addr,
                           PacketSocketFactory* socket_factory,
                           ProtocolType int_proto, ProtocolType ext_proto) {
    SocketAddress server_addr =
        int_proto == PROTO_TCP ? kTurnTcpIntAddr : kTurnUdpIntAddr;
    return CreateTurnPort(addr, socket_factory, int_proto, ext_proto,
                          server_addr);
  }
  TurnPort* CreateTurnPort(const SocketAddress& addr,
                           PacketSocketFactory* socket_factory,
                           ProtocolType int_proto, ProtocolType ext_proto,
                           const rtc::SocketAddress& server_addr) {
    return TurnPort::Create(main_, socket_factory, &network_, addr.ipaddr(), 0,
                            0, username_, password_,
                            ProtocolAddress(server_addr, int_proto),
                            kRelayCredentials, 0, std::string());
  }
  RelayPort* CreateGturnPort(const SocketAddress& addr,
                             ProtocolType int_proto, ProtocolType ext_proto) {
    RelayPort* port = CreateGturnPort(addr);
    SocketAddress addrs[] =
        { kRelayUdpIntAddr, kRelayTcpIntAddr, kRelaySslTcpIntAddr };
    port->AddServerAddress(ProtocolAddress(addrs[int_proto], int_proto));
    return port;
  }
  RelayPort* CreateGturnPort(const SocketAddress& addr) {
    // TODO(pthatcher):  Remove GTURN.
    return RelayPort::Create(main_, &socket_factory_, &network_,
                             addr.ipaddr(), 0, 0,
                             username_, password_);
    // TODO: Add an external address for ext_proto, so that the
    // other side can connect to this port using a non-UDP protocol.
  }
  rtc::NATServer* CreateNatServer(const SocketAddress& addr,
                                        rtc::NATType type) {
    return new rtc::NATServer(type, ss_.get(), addr, addr, ss_.get(), addr);
  }
  static const char* StunName(NATType type) {
    switch (type) {
      case NAT_OPEN_CONE:       return "stun(open cone)";
      case NAT_ADDR_RESTRICTED: return "stun(addr restricted)";
      case NAT_PORT_RESTRICTED: return "stun(port restricted)";
      case NAT_SYMMETRIC:       return "stun(symmetric)";
      default:                  return "stun(?)";
    }
  }
  static const char* RelayName(RelayType type, ProtocolType proto) {
    if (type == RELAY_TURN) {
      switch (proto) {
        case PROTO_UDP:           return "turn(udp)";
        case PROTO_TCP:           return "turn(tcp)";
        case PROTO_SSLTCP:        return "turn(ssltcp)";
        default:                  return "turn(?)";
      }
    } else {
      switch (proto) {
        case PROTO_UDP:           return "gturn(udp)";
        case PROTO_TCP:           return "gturn(tcp)";
        case PROTO_SSLTCP:        return "gturn(ssltcp)";
        default:                  return "gturn(?)";
      }
    }
  }

  void SetNetworkType(rtc::AdapterType adapter_type) {
    network_.set_type(adapter_type);
  }

  void TestCrossFamilyPorts(int type);

  void ExpectPortsCanConnect(bool can_connect, Port* p1, Port* p2);

  // This does all the work and then deletes |port1| and |port2|.
  void TestConnectivity(const char* name1, Port* port1,
                        const char* name2, Port* port2,
                        bool accept, bool same_addr1,
                        bool same_addr2, bool possible);

  // This connects the provided channels which have already started.  |ch1|
  // should have its Connection created (either through CreateConnection() or
  // TCP reconnecting mechanism before entering this function.
  void ConnectStartedChannels(TestChannel* ch1, TestChannel* ch2) {
    ASSERT_TRUE(ch1->conn());
    EXPECT_TRUE_WAIT(ch1->conn()->connected(), kTimeout);  // for TCP connect
    ch1->Ping();
    WAIT(!ch2->remote_address().IsNil(), kTimeout);

    // Send a ping from dst to src.
    ch2->AcceptConnection(GetCandidate(ch1->port()));
    ch2->Ping();
    EXPECT_EQ_WAIT(Connection::STATE_WRITABLE, ch2->conn()->write_state(),
                   kTimeout);
  }

  // This connects and disconnects the provided channels in the same sequence as
  // TestConnectivity with all options set to |true|.  It does not delete either
  // channel.
  void StartConnectAndStopChannels(TestChannel* ch1, TestChannel* ch2) {
    // Acquire addresses.
    ch1->Start();
    ch2->Start();

    ch1->CreateConnection(GetCandidate(ch2->port()));
    ConnectStartedChannels(ch1, ch2);

    // Destroy the connections.
    ch1->Stop();
    ch2->Stop();
  }

  // This disconnects both end's Connection and make sure ch2 ready for new
  // connection.
  void DisconnectTcpTestChannels(TestChannel* ch1, TestChannel* ch2) {
    TCPConnection* tcp_conn1 = static_cast<TCPConnection*>(ch1->conn());
    TCPConnection* tcp_conn2 = static_cast<TCPConnection*>(ch2->conn());
    ASSERT_TRUE(
        ss_->CloseTcpConnections(tcp_conn1->socket()->GetLocalAddress(),
                                 tcp_conn2->socket()->GetLocalAddress()));

    // Wait for both OnClose are delivered.
    EXPECT_TRUE_WAIT(!ch1->conn()->connected(), kTimeout);
    EXPECT_TRUE_WAIT(!ch2->conn()->connected(), kTimeout);

    // Ensure redundant SignalClose events on TcpConnection won't break tcp
    // reconnection. Chromium will fire SignalClose for all outstanding IPC
    // packets during reconnection.
    tcp_conn1->socket()->SignalClose(tcp_conn1->socket(), 0);
    tcp_conn2->socket()->SignalClose(tcp_conn2->socket(), 0);

    // Speed up destroying ch2's connection such that the test is ready to
    // accept a new connection from ch1 before ch1's connection destroys itself.
    ch2->conn()->Destroy();
    EXPECT_TRUE_WAIT(ch2->conn() == NULL, kTimeout);
  }

  void TestTcpReconnect(bool ping_after_disconnected,
                        bool send_after_disconnected) {
    Port* port1 = CreateTcpPort(kLocalAddr1);
    port1->SetIceRole(cricket::ICEROLE_CONTROLLING);
    Port* port2 = CreateTcpPort(kLocalAddr2);
    port2->SetIceRole(cricket::ICEROLE_CONTROLLED);

    port1->set_component(cricket::ICE_CANDIDATE_COMPONENT_DEFAULT);
    port2->set_component(cricket::ICE_CANDIDATE_COMPONENT_DEFAULT);

    // Set up channels and ensure both ports will be deleted.
    TestChannel ch1(port1);
    TestChannel ch2(port2);
    EXPECT_EQ(0, ch1.complete_count());
    EXPECT_EQ(0, ch2.complete_count());

    ch1.Start();
    ch2.Start();
    ASSERT_EQ_WAIT(1, ch1.complete_count(), kTimeout);
    ASSERT_EQ_WAIT(1, ch2.complete_count(), kTimeout);

    // Initial connecting the channel, create connection on channel1.
    ch1.CreateConnection(GetCandidate(port2));
    ConnectStartedChannels(&ch1, &ch2);

    // Shorten the timeout period.
    const int kTcpReconnectTimeout = kTimeout;
    static_cast<TCPConnection*>(ch1.conn())
        ->set_reconnection_timeout(kTcpReconnectTimeout);
    static_cast<TCPConnection*>(ch2.conn())
        ->set_reconnection_timeout(kTcpReconnectTimeout);

    EXPECT_FALSE(ch1.connection_ready_to_send());
    EXPECT_FALSE(ch2.connection_ready_to_send());

    // Once connected, disconnect them.
    DisconnectTcpTestChannels(&ch1, &ch2);

    if (send_after_disconnected || ping_after_disconnected) {
      if (send_after_disconnected) {
        // First SendData after disconnect should fail but will trigger
        // reconnect.
        EXPECT_EQ(-1, ch1.SendData(data, static_cast<int>(strlen(data))));
      }

      if (ping_after_disconnected) {
        // Ping should trigger reconnect.
        ch1.Ping();
      }

      // Wait for channel's outgoing TCPConnection connected.
      EXPECT_TRUE_WAIT(ch1.conn()->connected(), kTimeout);

      // Verify that we could still connect channels.
      ConnectStartedChannels(&ch1, &ch2);
      EXPECT_TRUE_WAIT(ch1.connection_ready_to_send(),
                       kTcpReconnectTimeout);
      // Channel2 is the passive one so a new connection is created during
      // reconnect. This new connection should never have issued EWOULDBLOCK
      // hence the connection_ready_to_send() should be false.
      EXPECT_FALSE(ch2.connection_ready_to_send());
    } else {
      EXPECT_EQ(ch1.conn()->write_state(), Connection::STATE_WRITABLE);
      // Since the reconnection never happens, the connections should have been
      // destroyed after the timeout.
      EXPECT_TRUE_WAIT(!ch1.conn(), kTcpReconnectTimeout + kTimeout);
      EXPECT_TRUE(!ch2.conn());
    }

    // Tear down and ensure that goes smoothly.
    ch1.Stop();
    ch2.Stop();
    EXPECT_TRUE_WAIT(ch1.conn() == NULL, kTimeout);
    EXPECT_TRUE_WAIT(ch2.conn() == NULL, kTimeout);
  }

  IceMessage* CreateStunMessage(int type) {
    IceMessage* msg = new IceMessage();
    msg->SetType(type);
    msg->SetTransactionID("TESTTESTTEST");
    return msg;
  }
  IceMessage* CreateStunMessageWithUsername(int type,
                                            const std::string& username) {
    IceMessage* msg = CreateStunMessage(type);
    msg->AddAttribute(
        new StunByteStringAttribute(STUN_ATTR_USERNAME, username));
    return msg;
  }
  TestPort* CreateTestPort(const rtc::SocketAddress& addr,
                           const std::string& username,
                           const std::string& password) {
    TestPort* port =  new TestPort(main_, "test", &socket_factory_, &network_,
                                   addr.ipaddr(), 0, 0, username, password);
    port->SignalRoleConflict.connect(this, &PortTest::OnRoleConflict);
    return port;
  }
  TestPort* CreateTestPort(const rtc::SocketAddress& addr,
                           const std::string& username,
                           const std::string& password,
                           cricket::IceRole role,
                           int tiebreaker) {
    TestPort* port = CreateTestPort(addr, username, password);
    port->SetIceRole(role);
    port->SetIceTiebreaker(tiebreaker);
    return port;
  }

  void OnRoleConflict(PortInterface* port) {
    role_conflict_ = true;
  }
  bool role_conflict() const { return role_conflict_; }

  void ConnectToSignalDestroyed(PortInterface* port) {
    port->SignalDestroyed.connect(this, &PortTest::OnDestroyed);
  }

  void OnDestroyed(PortInterface* port) {
    destroyed_ = true;
  }
  bool destroyed() const { return destroyed_; }

  rtc::BasicPacketSocketFactory* nat_socket_factory1() {
    return &nat_socket_factory1_;
  }

 protected:
  rtc::VirtualSocketServer* vss() { return ss_.get(); }

 private:
  rtc::Thread* main_;
  std::unique_ptr<rtc::PhysicalSocketServer> pss_;
  std::unique_ptr<rtc::VirtualSocketServer> ss_;
  rtc::SocketServerScope ss_scope_;
  rtc::Network network_;
  rtc::BasicPacketSocketFactory socket_factory_;
  std::unique_ptr<rtc::NATServer> nat_server1_;
  std::unique_ptr<rtc::NATServer> nat_server2_;
  rtc::NATSocketFactory nat_factory1_;
  rtc::NATSocketFactory nat_factory2_;
  rtc::BasicPacketSocketFactory nat_socket_factory1_;
  rtc::BasicPacketSocketFactory nat_socket_factory2_;
  std::unique_ptr<TestStunServer> stun_server_;
  TestTurnServer turn_server_;
  TestRelayServer relay_server_;
  std::string username_;
  std::string password_;
  bool role_conflict_;
  bool destroyed_;
};

void PortTest::TestConnectivity(const char* name1, Port* port1,
                                const char* name2, Port* port2,
                                bool accept, bool same_addr1,
                                bool same_addr2, bool possible) {
  LOG(LS_INFO) << "Test: " << name1 << " to " << name2 << ": ";
  port1->set_component(cricket::ICE_CANDIDATE_COMPONENT_DEFAULT);
  port2->set_component(cricket::ICE_CANDIDATE_COMPONENT_DEFAULT);

  // Set up channels and ensure both ports will be deleted.
  TestChannel ch1(port1);
  TestChannel ch2(port2);
  EXPECT_EQ(0, ch1.complete_count());
  EXPECT_EQ(0, ch2.complete_count());

  // Acquire addresses.
  ch1.Start();
  ch2.Start();
  ASSERT_EQ_WAIT(1, ch1.complete_count(), kTimeout);
  ASSERT_EQ_WAIT(1, ch2.complete_count(), kTimeout);

  // Send a ping from src to dst. This may or may not make it.
  ch1.CreateConnection(GetCandidate(port2));
  ASSERT_TRUE(ch1.conn() != NULL);
  EXPECT_TRUE_WAIT(ch1.conn()->connected(), kTimeout);  // for TCP connect
  ch1.Ping();
  WAIT(!ch2.remote_address().IsNil(), kTimeout);

  if (accept) {
    // We are able to send a ping from src to dst. This is the case when
    // sending to UDP ports and cone NATs.
    EXPECT_TRUE(ch1.remote_address().IsNil());
    EXPECT_EQ(ch2.remote_fragment(), port1->username_fragment());

    // Ensure the ping came from the same address used for src.
    // This is the case unless the source NAT was symmetric.
    if (same_addr1) EXPECT_EQ(ch2.remote_address(), GetAddress(port1));
    EXPECT_TRUE(same_addr2);

    // Send a ping from dst to src.
    ch2.AcceptConnection(GetCandidate(port1));
    ASSERT_TRUE(ch2.conn() != NULL);
    ch2.Ping();
    EXPECT_EQ_WAIT(Connection::STATE_WRITABLE, ch2.conn()->write_state(),
                   kTimeout);
  } else {
    // We can't send a ping from src to dst, so flip it around. This will happen
    // when the destination NAT is addr/port restricted or symmetric.
    EXPECT_TRUE(ch1.remote_address().IsNil());
    EXPECT_TRUE(ch2.remote_address().IsNil());

    // Send a ping from dst to src. Again, this may or may not make it.
    ch2.CreateConnection(GetCandidate(port1));
    ASSERT_TRUE(ch2.conn() != NULL);
    ch2.Ping();
    WAIT(ch2.conn()->write_state() == Connection::STATE_WRITABLE, kTimeout);

    if (same_addr1 && same_addr2) {
      // The new ping got back to the source.
      EXPECT_TRUE(ch1.conn()->receiving());
      EXPECT_EQ(Connection::STATE_WRITABLE, ch2.conn()->write_state());

      // First connection may not be writable if the first ping did not get
      // through.  So we will have to do another.
      if (ch1.conn()->write_state() == Connection::STATE_WRITE_INIT) {
        ch1.Ping();
        EXPECT_EQ_WAIT(Connection::STATE_WRITABLE, ch1.conn()->write_state(),
                       kTimeout);
      }
    } else if (!same_addr1 && possible) {
      // The new ping went to the candidate address, but that address was bad.
      // This will happen when the source NAT is symmetric.
      EXPECT_TRUE(ch1.remote_address().IsNil());
      EXPECT_TRUE(ch2.remote_address().IsNil());

      // However, since we have now sent a ping to the source IP, we should be
      // able to get a ping from it. This gives us the real source address.
      ch1.Ping();
      EXPECT_TRUE_WAIT(!ch2.remote_address().IsNil(), kTimeout);
      EXPECT_FALSE(ch2.conn()->receiving());
      EXPECT_TRUE(ch1.remote_address().IsNil());

      // Pick up the actual address and establish the connection.
      ch2.AcceptConnection(GetCandidate(port1));
      ASSERT_TRUE(ch2.conn() != NULL);
      ch2.Ping();
      EXPECT_EQ_WAIT(Connection::STATE_WRITABLE, ch2.conn()->write_state(),
                     kTimeout);
    } else if (!same_addr2 && possible) {
      // The new ping came in, but from an unexpected address. This will happen
      // when the destination NAT is symmetric.
      EXPECT_FALSE(ch1.remote_address().IsNil());
      EXPECT_FALSE(ch1.conn()->receiving());

      // Update our address and complete the connection.
      ch1.AcceptConnection(GetCandidate(port2));
      ch1.Ping();
      EXPECT_EQ_WAIT(Connection::STATE_WRITABLE, ch1.conn()->write_state(),
                     kTimeout);
    } else {  // (!possible)
      // There should be s no way for the pings to reach each other. Check it.
      EXPECT_TRUE(ch1.remote_address().IsNil());
      EXPECT_TRUE(ch2.remote_address().IsNil());
      ch1.Ping();
      WAIT(!ch2.remote_address().IsNil(), kTimeout);
      EXPECT_TRUE(ch1.remote_address().IsNil());
      EXPECT_TRUE(ch2.remote_address().IsNil());
    }
  }

  // Everything should be good, unless we know the situation is impossible.
  ASSERT_TRUE(ch1.conn() != NULL);
  ASSERT_TRUE(ch2.conn() != NULL);
  if (possible) {
    EXPECT_TRUE(ch1.conn()->receiving());
    EXPECT_EQ(Connection::STATE_WRITABLE, ch1.conn()->write_state());
    EXPECT_TRUE(ch2.conn()->receiving());
    EXPECT_EQ(Connection::STATE_WRITABLE, ch2.conn()->write_state());
  } else {
    EXPECT_FALSE(ch1.conn()->receiving());
    EXPECT_NE(Connection::STATE_WRITABLE, ch1.conn()->write_state());
    EXPECT_FALSE(ch2.conn()->receiving());
    EXPECT_NE(Connection::STATE_WRITABLE, ch2.conn()->write_state());
  }

  // Tear down and ensure that goes smoothly.
  ch1.Stop();
  ch2.Stop();
  EXPECT_TRUE_WAIT(ch1.conn() == NULL, kTimeout);
  EXPECT_TRUE_WAIT(ch2.conn() == NULL, kTimeout);
}

class FakePacketSocketFactory : public rtc::PacketSocketFactory {
 public:
  FakePacketSocketFactory()
      : next_udp_socket_(NULL),
        next_server_tcp_socket_(NULL),
        next_client_tcp_socket_(NULL) {
  }
  ~FakePacketSocketFactory() override { }

  AsyncPacketSocket* CreateUdpSocket(const SocketAddress& address,
                                     uint16_t min_port,
                                     uint16_t max_port) override {
    EXPECT_TRUE(next_udp_socket_ != NULL);
    AsyncPacketSocket* result = next_udp_socket_;
    next_udp_socket_ = NULL;
    return result;
  }

  AsyncPacketSocket* CreateServerTcpSocket(const SocketAddress& local_address,
                                           uint16_t min_port,
                                           uint16_t max_port,
                                           int opts) override {
    EXPECT_TRUE(next_server_tcp_socket_ != NULL);
    AsyncPacketSocket* result = next_server_tcp_socket_;
    next_server_tcp_socket_ = NULL;
    return result;
  }

  // TODO: |proxy_info| and |user_agent| should be set
  // per-factory and not when socket is created.
  AsyncPacketSocket* CreateClientTcpSocket(const SocketAddress& local_address,
                                           const SocketAddress& remote_address,
                                           const rtc::ProxyInfo& proxy_info,
                                           const std::string& user_agent,
                                           int opts) override {
    EXPECT_TRUE(next_client_tcp_socket_ != NULL);
    AsyncPacketSocket* result = next_client_tcp_socket_;
    next_client_tcp_socket_ = NULL;
    return result;
  }

  void set_next_udp_socket(AsyncPacketSocket* next_udp_socket) {
    next_udp_socket_ = next_udp_socket;
  }
  void set_next_server_tcp_socket(AsyncPacketSocket* next_server_tcp_socket) {
    next_server_tcp_socket_ = next_server_tcp_socket;
  }
  void set_next_client_tcp_socket(AsyncPacketSocket* next_client_tcp_socket) {
    next_client_tcp_socket_ = next_client_tcp_socket;
  }
  rtc::AsyncResolverInterface* CreateAsyncResolver() override {
    return NULL;
  }

 private:
  AsyncPacketSocket* next_udp_socket_;
  AsyncPacketSocket* next_server_tcp_socket_;
  AsyncPacketSocket* next_client_tcp_socket_;
};

class FakeAsyncPacketSocket : public AsyncPacketSocket {
 public:
  // Returns current local address. Address may be set to NULL if the
  // socket is not bound yet (GetState() returns STATE_BINDING).
  virtual SocketAddress GetLocalAddress() const {
    return SocketAddress();
  }

  // Returns remote address. Returns zeroes if this is not a client TCP socket.
  virtual SocketAddress GetRemoteAddress() const {
    return SocketAddress();
  }

  // Send a packet.
  virtual int Send(const void *pv, size_t cb,
                   const rtc::PacketOptions& options) {
    return static_cast<int>(cb);
  }
  virtual int SendTo(const void *pv, size_t cb, const SocketAddress& addr,
                     const rtc::PacketOptions& options) {
    return static_cast<int>(cb);
  }
  virtual int Close() {
    return 0;
  }

  virtual State GetState() const { return state_; }
  virtual int GetOption(Socket::Option opt, int* value) { return 0; }
  virtual int SetOption(Socket::Option opt, int value) { return 0; }
  virtual int GetError() const { return 0; }
  virtual void SetError(int error) { }

  void set_state(State state) { state_ = state; }

 private:
  State state_;
};

// Local -> XXXX
TEST_F(PortTest, TestLocalToLocal) {
  TestLocalToLocal();
}

TEST_F(PortTest, TestLocalToConeNat) {
  TestLocalToStun(NAT_OPEN_CONE);
}

TEST_F(PortTest, TestLocalToARNat) {
  TestLocalToStun(NAT_ADDR_RESTRICTED);
}

TEST_F(PortTest, TestLocalToPRNat) {
  TestLocalToStun(NAT_PORT_RESTRICTED);
}

TEST_F(PortTest, TestLocalToSymNat) {
  TestLocalToStun(NAT_SYMMETRIC);
}

// Flaky: https://code.google.com/p/webrtc/issues/detail?id=3316.
TEST_F(PortTest, DISABLED_TestLocalToTurn) {
  TestLocalToRelay(RELAY_TURN, PROTO_UDP);
}

TEST_F(PortTest, TestLocalToGturn) {
  TestLocalToRelay(RELAY_GTURN, PROTO_UDP);
}

TEST_F(PortTest, TestLocalToTcpGturn) {
  TestLocalToRelay(RELAY_GTURN, PROTO_TCP);
}

TEST_F(PortTest, TestLocalToSslTcpGturn) {
  TestLocalToRelay(RELAY_GTURN, PROTO_SSLTCP);
}

// Cone NAT -> XXXX
TEST_F(PortTest, TestConeNatToLocal) {
  TestStunToLocal(NAT_OPEN_CONE);
}

TEST_F(PortTest, TestConeNatToConeNat) {
  TestStunToStun(NAT_OPEN_CONE, NAT_OPEN_CONE);
}

TEST_F(PortTest, TestConeNatToARNat) {
  TestStunToStun(NAT_OPEN_CONE, NAT_ADDR_RESTRICTED);
}

TEST_F(PortTest, TestConeNatToPRNat) {
  TestStunToStun(NAT_OPEN_CONE, NAT_PORT_RESTRICTED);
}

TEST_F(PortTest, TestConeNatToSymNat) {
  TestStunToStun(NAT_OPEN_CONE, NAT_SYMMETRIC);
}

TEST_F(PortTest, TestConeNatToTurn) {
  TestStunToRelay(NAT_OPEN_CONE, RELAY_TURN, PROTO_UDP);
}

TEST_F(PortTest, TestConeNatToGturn) {
  TestStunToRelay(NAT_OPEN_CONE, RELAY_GTURN, PROTO_UDP);
}

TEST_F(PortTest, TestConeNatToTcpGturn) {
  TestStunToRelay(NAT_OPEN_CONE, RELAY_GTURN, PROTO_TCP);
}

// Address-restricted NAT -> XXXX
TEST_F(PortTest, TestARNatToLocal) {
  TestStunToLocal(NAT_ADDR_RESTRICTED);
}

TEST_F(PortTest, TestARNatToConeNat) {
  TestStunToStun(NAT_ADDR_RESTRICTED, NAT_OPEN_CONE);
}

TEST_F(PortTest, TestARNatToARNat) {
  TestStunToStun(NAT_ADDR_RESTRICTED, NAT_ADDR_RESTRICTED);
}

TEST_F(PortTest, TestARNatToPRNat) {
  TestStunToStun(NAT_ADDR_RESTRICTED, NAT_PORT_RESTRICTED);
}

TEST_F(PortTest, TestARNatToSymNat) {
  TestStunToStun(NAT_ADDR_RESTRICTED, NAT_SYMMETRIC);
}

TEST_F(PortTest, TestARNatToTurn) {
  TestStunToRelay(NAT_ADDR_RESTRICTED, RELAY_TURN, PROTO_UDP);
}

TEST_F(PortTest, TestARNatToGturn) {
  TestStunToRelay(NAT_ADDR_RESTRICTED, RELAY_GTURN, PROTO_UDP);
}

TEST_F(PortTest, TestARNATNatToTcpGturn) {
  TestStunToRelay(NAT_ADDR_RESTRICTED, RELAY_GTURN, PROTO_TCP);
}

// Port-restricted NAT -> XXXX
TEST_F(PortTest, TestPRNatToLocal) {
  TestStunToLocal(NAT_PORT_RESTRICTED);
}

TEST_F(PortTest, TestPRNatToConeNat) {
  TestStunToStun(NAT_PORT_RESTRICTED, NAT_OPEN_CONE);
}

TEST_F(PortTest, TestPRNatToARNat) {
  TestStunToStun(NAT_PORT_RESTRICTED, NAT_ADDR_RESTRICTED);
}

TEST_F(PortTest, TestPRNatToPRNat) {
  TestStunToStun(NAT_PORT_RESTRICTED, NAT_PORT_RESTRICTED);
}

TEST_F(PortTest, TestPRNatToSymNat) {
  // Will "fail"
  TestStunToStun(NAT_PORT_RESTRICTED, NAT_SYMMETRIC);
}

TEST_F(PortTest, TestPRNatToTurn) {
  TestStunToRelay(NAT_PORT_RESTRICTED, RELAY_TURN, PROTO_UDP);
}

TEST_F(PortTest, TestPRNatToGturn) {
  TestStunToRelay(NAT_PORT_RESTRICTED, RELAY_GTURN, PROTO_UDP);
}

TEST_F(PortTest, TestPRNatToTcpGturn) {
  TestStunToRelay(NAT_PORT_RESTRICTED, RELAY_GTURN, PROTO_TCP);
}

// Symmetric NAT -> XXXX
TEST_F(PortTest, TestSymNatToLocal) {
  TestStunToLocal(NAT_SYMMETRIC);
}

TEST_F(PortTest, TestSymNatToConeNat) {
  TestStunToStun(NAT_SYMMETRIC, NAT_OPEN_CONE);
}

TEST_F(PortTest, TestSymNatToARNat) {
  TestStunToStun(NAT_SYMMETRIC, NAT_ADDR_RESTRICTED);
}

TEST_F(PortTest, TestSymNatToPRNat) {
  // Will "fail"
  TestStunToStun(NAT_SYMMETRIC, NAT_PORT_RESTRICTED);
}

TEST_F(PortTest, TestSymNatToSymNat) {
  // Will "fail"
  TestStunToStun(NAT_SYMMETRIC, NAT_SYMMETRIC);
}

TEST_F(PortTest, TestSymNatToTurn) {
  TestStunToRelay(NAT_SYMMETRIC, RELAY_TURN, PROTO_UDP);
}

TEST_F(PortTest, TestSymNatToGturn) {
  TestStunToRelay(NAT_SYMMETRIC, RELAY_GTURN, PROTO_UDP);
}

TEST_F(PortTest, TestSymNatToTcpGturn) {
  TestStunToRelay(NAT_SYMMETRIC, RELAY_GTURN, PROTO_TCP);
}

// Outbound TCP -> XXXX
TEST_F(PortTest, TestTcpToTcp) {
  TestTcpToTcp();
}

TEST_F(PortTest, TestTcpReconnectOnSendPacket) {
  TestTcpReconnect(false /* ping */, true /* send */);
}

TEST_F(PortTest, TestTcpReconnectOnPing) {
  TestTcpReconnect(true /* ping */, false /* send */);
}

TEST_F(PortTest, TestTcpReconnectTimeout) {
  TestTcpReconnect(false /* ping */, false /* send */);
}

// Test when TcpConnection never connects, the OnClose() will be called to
// destroy the connection.
TEST_F(PortTest, TestTcpNeverConnect) {
  Port* port1 = CreateTcpPort(kLocalAddr1);
  port1->SetIceRole(cricket::ICEROLE_CONTROLLING);
  port1->set_component(cricket::ICE_CANDIDATE_COMPONENT_DEFAULT);

  // Set up a channel and ensure the port will be deleted.
  TestChannel ch1(port1);
  EXPECT_EQ(0, ch1.complete_count());

  ch1.Start();
  ASSERT_EQ_WAIT(1, ch1.complete_count(), kTimeout);

  std::unique_ptr<rtc::AsyncSocket> server(
      vss()->CreateAsyncSocket(kLocalAddr2.family(), SOCK_STREAM));
  // Bind but not listen.
  EXPECT_EQ(0, server->Bind(kLocalAddr2));

  Candidate c = GetCandidate(port1);
  c.set_address(server->GetLocalAddress());

  ch1.CreateConnection(c);
  EXPECT_TRUE(ch1.conn());
  EXPECT_TRUE_WAIT(!ch1.conn(), kTimeout);  // for TCP connect
}

/* TODO: Enable these once testrelayserver can accept external TCP.
TEST_F(PortTest, TestTcpToTcpRelay) {
  TestTcpToRelay(PROTO_TCP);
}

TEST_F(PortTest, TestTcpToSslTcpRelay) {
  TestTcpToRelay(PROTO_SSLTCP);
}
*/

// Outbound SSLTCP -> XXXX
/* TODO: Enable these once testrelayserver can accept external SSL.
TEST_F(PortTest, TestSslTcpToTcpRelay) {
  TestSslTcpToRelay(PROTO_TCP);
}

TEST_F(PortTest, TestSslTcpToSslTcpRelay) {
  TestSslTcpToRelay(PROTO_SSLTCP);
}
*/

// Test that a connection will be dead and deleted if
// i) it has never received anything for MIN_CONNECTION_LIFETIME milliseconds
//    since it was created, or
// ii) it has not received anything for DEAD_CONNECTION_RECEIVE_TIMEOUT
//     milliseconds since last receiving.
TEST_F(PortTest, TestConnectionDead) {
  UDPPort* port1 = CreateUdpPort(kLocalAddr1);
  UDPPort* port2 = CreateUdpPort(kLocalAddr2);
  TestChannel ch1(port1);
  TestChannel ch2(port2);
  // Acquire address.
  ch1.Start();
  ch2.Start();
  ASSERT_EQ_WAIT(1, ch1.complete_count(), kTimeout);
  ASSERT_EQ_WAIT(1, ch2.complete_count(), kTimeout);

  // Test case that the connection has never received anything.
  int64_t before_created = rtc::TimeMillis();
  ch1.CreateConnection(GetCandidate(port2));
  int64_t after_created = rtc::TimeMillis();
  Connection* conn = ch1.conn();
  ASSERT(conn != nullptr);
  // It is not dead if it is after MIN_CONNECTION_LIFETIME but not pruned.
  conn->UpdateState(after_created + MIN_CONNECTION_LIFETIME + 1);
  rtc::Thread::Current()->ProcessMessages(0);
  EXPECT_TRUE(ch1.conn() != nullptr);
  // It is not dead if it is before MIN_CONNECTION_LIFETIME and pruned.
  conn->UpdateState(before_created + MIN_CONNECTION_LIFETIME - 1);
  conn->Prune();
  rtc::Thread::Current()->ProcessMessages(0);
  EXPECT_TRUE(ch1.conn() != nullptr);
  // It will be dead after MIN_CONNECTION_LIFETIME and pruned.
  conn->UpdateState(after_created + MIN_CONNECTION_LIFETIME + 1);
  EXPECT_TRUE_WAIT(ch1.conn() == nullptr, kTimeout);

  // Test case that the connection has received something.
  // Create a connection again and receive a ping.
  ch1.CreateConnection(GetCandidate(port2));
  conn = ch1.conn();
  ASSERT(conn != nullptr);
  int64_t before_last_receiving = rtc::TimeMillis();
  conn->ReceivedPing();
  int64_t after_last_receiving = rtc::TimeMillis();
  // The connection will be dead after DEAD_CONNECTION_RECEIVE_TIMEOUT
  conn->UpdateState(
      before_last_receiving + DEAD_CONNECTION_RECEIVE_TIMEOUT - 1);
  rtc::Thread::Current()->ProcessMessages(100);
  EXPECT_TRUE(ch1.conn() != nullptr);
  conn->UpdateState(after_last_receiving + DEAD_CONNECTION_RECEIVE_TIMEOUT + 1);
  EXPECT_TRUE_WAIT(ch1.conn() == nullptr, kTimeout);
}

// This test case verifies standard ICE features in STUN messages. Currently it
// verifies Message Integrity attribute in STUN messages and username in STUN
// binding request will have colon (":") between remote and local username.
TEST_F(PortTest, TestLocalToLocalStandard) {
  UDPPort* port1 = CreateUdpPort(kLocalAddr1);
  port1->SetIceRole(cricket::ICEROLE_CONTROLLING);
  port1->SetIceTiebreaker(kTiebreaker1);
  UDPPort* port2 = CreateUdpPort(kLocalAddr2);
  port2->SetIceRole(cricket::ICEROLE_CONTROLLED);
  port2->SetIceTiebreaker(kTiebreaker2);
  // Same parameters as TestLocalToLocal above.
  TestConnectivity("udp", port1, "udp", port2, true, true, true, true);
}

// This test is trying to validate a successful and failure scenario in a
// loopback test when protocol is RFC5245. For success IceTiebreaker, username
// should remain equal to the request generated by the port and role of port
// must be in controlling.
TEST_F(PortTest, TestLoopbackCal) {
  std::unique_ptr<TestPort> lport(
      CreateTestPort(kLocalAddr1, "lfrag", "lpass"));
  lport->SetIceRole(cricket::ICEROLE_CONTROLLING);
  lport->SetIceTiebreaker(kTiebreaker1);
  lport->PrepareAddress();
  ASSERT_FALSE(lport->Candidates().empty());
  Connection* conn = lport->CreateConnection(lport->Candidates()[0],
                                             Port::ORIGIN_MESSAGE);
  conn->Ping(0);

  ASSERT_TRUE_WAIT(lport->last_stun_msg() != NULL, 1000);
  IceMessage* msg = lport->last_stun_msg();
  EXPECT_EQ(STUN_BINDING_REQUEST, msg->type());
  conn->OnReadPacket(lport->last_stun_buf()->data<char>(),
                     lport->last_stun_buf()->size(),
                     rtc::PacketTime());
  ASSERT_TRUE_WAIT(lport->last_stun_msg() != NULL, 1000);
  msg = lport->last_stun_msg();
  EXPECT_EQ(STUN_BINDING_RESPONSE, msg->type());

  // If the tiebreaker value is different from port, we expect a error
  // response.
  lport->Reset();
  lport->AddCandidateAddress(kLocalAddr2);
  // Creating a different connection as |conn| is receiving.
  Connection* conn1 = lport->CreateConnection(lport->Candidates()[1],
                                              Port::ORIGIN_MESSAGE);
  conn1->Ping(0);

  ASSERT_TRUE_WAIT(lport->last_stun_msg() != NULL, 1000);
  msg = lport->last_stun_msg();
  EXPECT_EQ(STUN_BINDING_REQUEST, msg->type());
  std::unique_ptr<IceMessage> modified_req(
      CreateStunMessage(STUN_BINDING_REQUEST));
  const StunByteStringAttribute* username_attr = msg->GetByteString(
      STUN_ATTR_USERNAME);
  modified_req->AddAttribute(new StunByteStringAttribute(
      STUN_ATTR_USERNAME, username_attr->GetString()));
  // To make sure we receive error response, adding tiebreaker less than
  // what's present in request.
  modified_req->AddAttribute(new StunUInt64Attribute(
      STUN_ATTR_ICE_CONTROLLING, kTiebreaker1 - 1));
  modified_req->AddMessageIntegrity("lpass");
  modified_req->AddFingerprint();

  lport->Reset();
  std::unique_ptr<ByteBufferWriter> buf(new ByteBufferWriter());
  WriteStunMessage(modified_req.get(), buf.get());
  conn1->OnReadPacket(buf->Data(), buf->Length(), rtc::PacketTime());
  ASSERT_TRUE_WAIT(lport->last_stun_msg() != NULL, 1000);
  msg = lport->last_stun_msg();
  EXPECT_EQ(STUN_BINDING_ERROR_RESPONSE, msg->type());
}

// This test verifies role conflict signal is received when there is
// conflict in the role. In this case both ports are in controlling and
// |rport| has higher tiebreaker value than |lport|. Since |lport| has lower
// value of tiebreaker, when it receives ping request from |rport| it will
// send role conflict signal.
TEST_F(PortTest, TestIceRoleConflict) {
  std::unique_ptr<TestPort> lport(
      CreateTestPort(kLocalAddr1, "lfrag", "lpass"));
  lport->SetIceRole(cricket::ICEROLE_CONTROLLING);
  lport->SetIceTiebreaker(kTiebreaker1);
  std::unique_ptr<TestPort> rport(
      CreateTestPort(kLocalAddr2, "rfrag", "rpass"));
  rport->SetIceRole(cricket::ICEROLE_CONTROLLING);
  rport->SetIceTiebreaker(kTiebreaker2);

  lport->PrepareAddress();
  rport->PrepareAddress();
  ASSERT_FALSE(lport->Candidates().empty());
  ASSERT_FALSE(rport->Candidates().empty());
  Connection* lconn = lport->CreateConnection(rport->Candidates()[0],
                                              Port::ORIGIN_MESSAGE);
  Connection* rconn = rport->CreateConnection(lport->Candidates()[0],
                                              Port::ORIGIN_MESSAGE);
  rconn->Ping(0);

  ASSERT_TRUE_WAIT(rport->last_stun_msg() != NULL, 1000);
  IceMessage* msg = rport->last_stun_msg();
  EXPECT_EQ(STUN_BINDING_REQUEST, msg->type());
  // Send rport binding request to lport.
  lconn->OnReadPacket(rport->last_stun_buf()->data<char>(),
                      rport->last_stun_buf()->size(),
                      rtc::PacketTime());

  ASSERT_TRUE_WAIT(lport->last_stun_msg() != NULL, 1000);
  EXPECT_EQ(STUN_BINDING_RESPONSE, lport->last_stun_msg()->type());
  EXPECT_TRUE(role_conflict());
}

TEST_F(PortTest, TestTcpNoDelay) {
  TCPPort* port1 = CreateTcpPort(kLocalAddr1);
  port1->SetIceRole(cricket::ICEROLE_CONTROLLING);
  int option_value = -1;
  int success = port1->GetOption(rtc::Socket::OPT_NODELAY,
                                 &option_value);
  ASSERT_EQ(0, success);  // GetOption() should complete successfully w/ 0
  ASSERT_EQ(1, option_value);
  delete port1;
}

TEST_F(PortTest, TestDelayedBindingUdp) {
  FakeAsyncPacketSocket *socket = new FakeAsyncPacketSocket();
  FakePacketSocketFactory socket_factory;

  socket_factory.set_next_udp_socket(socket);
  std::unique_ptr<UDPPort> port(CreateUdpPort(kLocalAddr1, &socket_factory));

  socket->set_state(AsyncPacketSocket::STATE_BINDING);
  port->PrepareAddress();

  EXPECT_EQ(0U, port->Candidates().size());
  socket->SignalAddressReady(socket, kLocalAddr2);

  EXPECT_EQ(1U, port->Candidates().size());
}

TEST_F(PortTest, TestDelayedBindingTcp) {
  FakeAsyncPacketSocket *socket = new FakeAsyncPacketSocket();
  FakePacketSocketFactory socket_factory;

  socket_factory.set_next_server_tcp_socket(socket);
  std::unique_ptr<TCPPort> port(CreateTcpPort(kLocalAddr1, &socket_factory));

  socket->set_state(AsyncPacketSocket::STATE_BINDING);
  port->PrepareAddress();

  EXPECT_EQ(0U, port->Candidates().size());
  socket->SignalAddressReady(socket, kLocalAddr2);

  EXPECT_EQ(1U, port->Candidates().size());
}

void PortTest::TestCrossFamilyPorts(int type) {
  FakePacketSocketFactory factory;
  std::unique_ptr<Port> ports[4];
  SocketAddress addresses[4] = {SocketAddress("192.168.1.3", 0),
                                SocketAddress("192.168.1.4", 0),
                                SocketAddress("2001:db8::1", 0),
                                SocketAddress("2001:db8::2", 0)};
  for (int i = 0; i < 4; i++) {
    FakeAsyncPacketSocket *socket = new FakeAsyncPacketSocket();
    if (type == SOCK_DGRAM) {
      factory.set_next_udp_socket(socket);
      ports[i].reset(CreateUdpPort(addresses[i], &factory));
    } else if (type == SOCK_STREAM) {
      factory.set_next_server_tcp_socket(socket);
      ports[i].reset(CreateTcpPort(addresses[i], &factory));
    }
    socket->set_state(AsyncPacketSocket::STATE_BINDING);
    socket->SignalAddressReady(socket, addresses[i]);
    ports[i]->PrepareAddress();
  }

  // IPv4 Port, connects to IPv6 candidate and then to IPv4 candidate.
  if (type == SOCK_STREAM) {
    FakeAsyncPacketSocket* clientsocket = new FakeAsyncPacketSocket();
    factory.set_next_client_tcp_socket(clientsocket);
  }
  Connection* c = ports[0]->CreateConnection(GetCandidate(ports[2].get()),
                                             Port::ORIGIN_MESSAGE);
  EXPECT_TRUE(NULL == c);
  EXPECT_EQ(0U, ports[0]->connections().size());
  c = ports[0]->CreateConnection(GetCandidate(ports[1].get()),
                                 Port::ORIGIN_MESSAGE);
  EXPECT_FALSE(NULL == c);
  EXPECT_EQ(1U, ports[0]->connections().size());

  // IPv6 Port, connects to IPv4 candidate and to IPv6 candidate.
  if (type == SOCK_STREAM) {
    FakeAsyncPacketSocket* clientsocket = new FakeAsyncPacketSocket();
    factory.set_next_client_tcp_socket(clientsocket);
  }
  c = ports[2]->CreateConnection(GetCandidate(ports[0].get()),
                                 Port::ORIGIN_MESSAGE);
  EXPECT_TRUE(NULL == c);
  EXPECT_EQ(0U, ports[2]->connections().size());
  c = ports[2]->CreateConnection(GetCandidate(ports[3].get()),
                                 Port::ORIGIN_MESSAGE);
  EXPECT_FALSE(NULL == c);
  EXPECT_EQ(1U, ports[2]->connections().size());
}

TEST_F(PortTest, TestSkipCrossFamilyTcp) {
  TestCrossFamilyPorts(SOCK_STREAM);
}

TEST_F(PortTest, TestSkipCrossFamilyUdp) {
  TestCrossFamilyPorts(SOCK_DGRAM);
}

void PortTest::ExpectPortsCanConnect(bool can_connect, Port* p1, Port* p2) {
  Connection* c = p1->CreateConnection(GetCandidate(p2),
                                       Port::ORIGIN_MESSAGE);
  if (can_connect) {
    EXPECT_FALSE(NULL == c);
    EXPECT_EQ(1U, p1->connections().size());
  } else {
    EXPECT_TRUE(NULL == c);
    EXPECT_EQ(0U, p1->connections().size());
  }
}

TEST_F(PortTest, TestUdpV6CrossTypePorts) {
  FakePacketSocketFactory factory;
  std::unique_ptr<Port> ports[4];
  SocketAddress addresses[4] = {SocketAddress("2001:db8::1", 0),
                                SocketAddress("fe80::1", 0),
                                SocketAddress("fe80::2", 0),
                                SocketAddress("::1", 0)};
  for (int i = 0; i < 4; i++) {
    FakeAsyncPacketSocket *socket = new FakeAsyncPacketSocket();
    factory.set_next_udp_socket(socket);
    ports[i].reset(CreateUdpPort(addresses[i], &factory));
    socket->set_state(AsyncPacketSocket::STATE_BINDING);
    socket->SignalAddressReady(socket, addresses[i]);
    ports[i]->PrepareAddress();
  }

  Port* standard = ports[0].get();
  Port* link_local1 = ports[1].get();
  Port* link_local2 = ports[2].get();
  Port* localhost = ports[3].get();

  ExpectPortsCanConnect(false, link_local1, standard);
  ExpectPortsCanConnect(false, standard, link_local1);
  ExpectPortsCanConnect(false, link_local1, localhost);
  ExpectPortsCanConnect(false, localhost, link_local1);

  ExpectPortsCanConnect(true, link_local1, link_local2);
  ExpectPortsCanConnect(true, localhost, standard);
  ExpectPortsCanConnect(true, standard, localhost);
}

// This test verifies DSCP value set through SetOption interface can be
// get through DefaultDscpValue.
TEST_F(PortTest, TestDefaultDscpValue) {
  int dscp;
  std::unique_ptr<UDPPort> udpport(CreateUdpPort(kLocalAddr1));
  EXPECT_EQ(0, udpport->SetOption(rtc::Socket::OPT_DSCP,
                                  rtc::DSCP_CS6));
  EXPECT_EQ(0, udpport->GetOption(rtc::Socket::OPT_DSCP, &dscp));
  std::unique_ptr<TCPPort> tcpport(CreateTcpPort(kLocalAddr1));
  EXPECT_EQ(0, tcpport->SetOption(rtc::Socket::OPT_DSCP,
                                 rtc::DSCP_AF31));
  EXPECT_EQ(0, tcpport->GetOption(rtc::Socket::OPT_DSCP, &dscp));
  EXPECT_EQ(rtc::DSCP_AF31, dscp);
  std::unique_ptr<StunPort> stunport(
      CreateStunPort(kLocalAddr1, nat_socket_factory1()));
  EXPECT_EQ(0, stunport->SetOption(rtc::Socket::OPT_DSCP,
                                  rtc::DSCP_AF41));
  EXPECT_EQ(0, stunport->GetOption(rtc::Socket::OPT_DSCP, &dscp));
  EXPECT_EQ(rtc::DSCP_AF41, dscp);
  std::unique_ptr<TurnPort> turnport1(
      CreateTurnPort(kLocalAddr1, nat_socket_factory1(), PROTO_UDP, PROTO_UDP));
  // Socket is created in PrepareAddress.
  turnport1->PrepareAddress();
  EXPECT_EQ(0, turnport1->SetOption(rtc::Socket::OPT_DSCP,
                                  rtc::DSCP_CS7));
  EXPECT_EQ(0, turnport1->GetOption(rtc::Socket::OPT_DSCP, &dscp));
  EXPECT_EQ(rtc::DSCP_CS7, dscp);
  // This will verify correct value returned without the socket.
  std::unique_ptr<TurnPort> turnport2(
      CreateTurnPort(kLocalAddr1, nat_socket_factory1(), PROTO_UDP, PROTO_UDP));
  EXPECT_EQ(0, turnport2->SetOption(rtc::Socket::OPT_DSCP,
                                  rtc::DSCP_CS6));
  EXPECT_EQ(0, turnport2->GetOption(rtc::Socket::OPT_DSCP, &dscp));
  EXPECT_EQ(rtc::DSCP_CS6, dscp);
}

// Test sending STUN messages.
TEST_F(PortTest, TestSendStunMessage) {
  std::unique_ptr<TestPort> lport(
      CreateTestPort(kLocalAddr1, "lfrag", "lpass"));
  std::unique_ptr<TestPort> rport(
      CreateTestPort(kLocalAddr2, "rfrag", "rpass"));
  lport->SetIceRole(cricket::ICEROLE_CONTROLLING);
  lport->SetIceTiebreaker(kTiebreaker1);
  rport->SetIceRole(cricket::ICEROLE_CONTROLLED);
  rport->SetIceTiebreaker(kTiebreaker2);

  // Send a fake ping from lport to rport.
  lport->PrepareAddress();
  rport->PrepareAddress();
  ASSERT_FALSE(rport->Candidates().empty());
  Connection* lconn = lport->CreateConnection(
      rport->Candidates()[0], Port::ORIGIN_MESSAGE);
  Connection* rconn = rport->CreateConnection(
      lport->Candidates()[0], Port::ORIGIN_MESSAGE);
  lconn->Ping(0);

  // Check that it's a proper BINDING-REQUEST.
  ASSERT_TRUE_WAIT(lport->last_stun_msg() != NULL, 1000);
  IceMessage* msg = lport->last_stun_msg();
  EXPECT_EQ(STUN_BINDING_REQUEST, msg->type());
  EXPECT_FALSE(msg->IsLegacy());
  const StunByteStringAttribute* username_attr =
      msg->GetByteString(STUN_ATTR_USERNAME);
  ASSERT_TRUE(username_attr != NULL);
  const StunUInt32Attribute* priority_attr = msg->GetUInt32(STUN_ATTR_PRIORITY);
  ASSERT_TRUE(priority_attr != NULL);
  EXPECT_EQ(kDefaultPrflxPriority, priority_attr->value());
  EXPECT_EQ("rfrag:lfrag", username_attr->GetString());
  EXPECT_TRUE(msg->GetByteString(STUN_ATTR_MESSAGE_INTEGRITY) != NULL);
  EXPECT_TRUE(StunMessage::ValidateMessageIntegrity(
      lport->last_stun_buf()->data<char>(), lport->last_stun_buf()->size(),
      "rpass"));
  const StunUInt64Attribute* ice_controlling_attr =
      msg->GetUInt64(STUN_ATTR_ICE_CONTROLLING);
  ASSERT_TRUE(ice_controlling_attr != NULL);
  EXPECT_EQ(lport->IceTiebreaker(), ice_controlling_attr->value());
  EXPECT_TRUE(msg->GetByteString(STUN_ATTR_ICE_CONTROLLED) == NULL);
  EXPECT_TRUE(msg->GetByteString(STUN_ATTR_USE_CANDIDATE) != NULL);
  EXPECT_TRUE(msg->GetUInt32(STUN_ATTR_FINGERPRINT) != NULL);
  EXPECT_TRUE(StunMessage::ValidateFingerprint(
      lport->last_stun_buf()->data<char>(), lport->last_stun_buf()->size()));

  // Request should not include ping count.
  ASSERT_TRUE(msg->GetUInt32(STUN_ATTR_RETRANSMIT_COUNT) == NULL);

  // Save a copy of the BINDING-REQUEST for use below.
  std::unique_ptr<IceMessage> request(CopyStunMessage(msg));

  // Respond with a BINDING-RESPONSE.
  rport->SendBindingResponse(request.get(), lport->Candidates()[0].address());
  msg = rport->last_stun_msg();
  ASSERT_TRUE(msg != NULL);
  EXPECT_EQ(STUN_BINDING_RESPONSE, msg->type());


  EXPECT_FALSE(msg->IsLegacy());
  const StunAddressAttribute* addr_attr = msg->GetAddress(
      STUN_ATTR_XOR_MAPPED_ADDRESS);
  ASSERT_TRUE(addr_attr != NULL);
  EXPECT_EQ(lport->Candidates()[0].address(), addr_attr->GetAddress());
  EXPECT_TRUE(msg->GetByteString(STUN_ATTR_MESSAGE_INTEGRITY) != NULL);
  EXPECT_TRUE(StunMessage::ValidateMessageIntegrity(
      rport->last_stun_buf()->data<char>(), rport->last_stun_buf()->size(),
      "rpass"));
  EXPECT_TRUE(msg->GetUInt32(STUN_ATTR_FINGERPRINT) != NULL);
  EXPECT_TRUE(StunMessage::ValidateFingerprint(
      lport->last_stun_buf()->data<char>(), lport->last_stun_buf()->size()));
  // No USERNAME or PRIORITY in ICE responses.
  EXPECT_TRUE(msg->GetByteString(STUN_ATTR_USERNAME) == NULL);
  EXPECT_TRUE(msg->GetByteString(STUN_ATTR_PRIORITY) == NULL);
  EXPECT_TRUE(msg->GetByteString(STUN_ATTR_MAPPED_ADDRESS) == NULL);
  EXPECT_TRUE(msg->GetByteString(STUN_ATTR_ICE_CONTROLLING) == NULL);
  EXPECT_TRUE(msg->GetByteString(STUN_ATTR_ICE_CONTROLLED) == NULL);
  EXPECT_TRUE(msg->GetByteString(STUN_ATTR_USE_CANDIDATE) == NULL);

  // Response should not include ping count.
  ASSERT_TRUE(msg->GetUInt32(STUN_ATTR_RETRANSMIT_COUNT) == NULL);

  // Respond with a BINDING-ERROR-RESPONSE. This wouldn't happen in real life,
  // but we can do it here.
  rport->SendBindingErrorResponse(request.get(),
                                  lport->Candidates()[0].address(),
                                  STUN_ERROR_SERVER_ERROR,
                                  STUN_ERROR_REASON_SERVER_ERROR);
  msg = rport->last_stun_msg();
  ASSERT_TRUE(msg != NULL);
  EXPECT_EQ(STUN_BINDING_ERROR_RESPONSE, msg->type());
  EXPECT_FALSE(msg->IsLegacy());
  const StunErrorCodeAttribute* error_attr = msg->GetErrorCode();
  ASSERT_TRUE(error_attr != NULL);
  EXPECT_EQ(STUN_ERROR_SERVER_ERROR, error_attr->code());
  EXPECT_EQ(std::string(STUN_ERROR_REASON_SERVER_ERROR), error_attr->reason());
  EXPECT_TRUE(msg->GetByteString(STUN_ATTR_MESSAGE_INTEGRITY) != NULL);
  EXPECT_TRUE(StunMessage::ValidateMessageIntegrity(
      rport->last_stun_buf()->data<char>(), rport->last_stun_buf()->size(),
      "rpass"));
  EXPECT_TRUE(msg->GetUInt32(STUN_ATTR_FINGERPRINT) != NULL);
  EXPECT_TRUE(StunMessage::ValidateFingerprint(
      lport->last_stun_buf()->data<char>(), lport->last_stun_buf()->size()));
  // No USERNAME with ICE.
  EXPECT_TRUE(msg->GetByteString(STUN_ATTR_USERNAME) == NULL);
  EXPECT_TRUE(msg->GetByteString(STUN_ATTR_PRIORITY) == NULL);

  // Testing STUN binding requests from rport --> lport, having ICE_CONTROLLED
  // and (incremented) RETRANSMIT_COUNT attributes.
  rport->Reset();
  rport->set_send_retransmit_count_attribute(true);
  rconn->Ping(0);
  rconn->Ping(0);
  rconn->Ping(0);
  ASSERT_TRUE_WAIT(rport->last_stun_msg() != NULL, 1000);
  msg = rport->last_stun_msg();
  EXPECT_EQ(STUN_BINDING_REQUEST, msg->type());
  const StunUInt64Attribute* ice_controlled_attr =
      msg->GetUInt64(STUN_ATTR_ICE_CONTROLLED);
  ASSERT_TRUE(ice_controlled_attr != NULL);
  EXPECT_EQ(rport->IceTiebreaker(), ice_controlled_attr->value());
  EXPECT_TRUE(msg->GetByteString(STUN_ATTR_USE_CANDIDATE) == NULL);

  // Request should include ping count.
  const StunUInt32Attribute* retransmit_attr =
      msg->GetUInt32(STUN_ATTR_RETRANSMIT_COUNT);
  ASSERT_TRUE(retransmit_attr != NULL);
  EXPECT_EQ(2U, retransmit_attr->value());

  // Respond with a BINDING-RESPONSE.
  request.reset(CopyStunMessage(msg));
  lport->SendBindingResponse(request.get(), rport->Candidates()[0].address());
  msg = lport->last_stun_msg();

  // Response should include same ping count.
  retransmit_attr = msg->GetUInt32(STUN_ATTR_RETRANSMIT_COUNT);
  ASSERT_TRUE(retransmit_attr != NULL);
  EXPECT_EQ(2U, retransmit_attr->value());
}

TEST_F(PortTest, TestUseCandidateAttribute) {
  std::unique_ptr<TestPort> lport(
      CreateTestPort(kLocalAddr1, "lfrag", "lpass"));
  std::unique_ptr<TestPort> rport(
      CreateTestPort(kLocalAddr2, "rfrag", "rpass"));
  lport->SetIceRole(cricket::ICEROLE_CONTROLLING);
  lport->SetIceTiebreaker(kTiebreaker1);
  rport->SetIceRole(cricket::ICEROLE_CONTROLLED);
  rport->SetIceTiebreaker(kTiebreaker2);

  // Send a fake ping from lport to rport.
  lport->PrepareAddress();
  rport->PrepareAddress();
  ASSERT_FALSE(rport->Candidates().empty());
  Connection* lconn = lport->CreateConnection(
      rport->Candidates()[0], Port::ORIGIN_MESSAGE);
  lconn->Ping(0);
  ASSERT_TRUE_WAIT(lport->last_stun_msg() != NULL, 1000);
  IceMessage* msg = lport->last_stun_msg();
  const StunUInt64Attribute* ice_controlling_attr =
      msg->GetUInt64(STUN_ATTR_ICE_CONTROLLING);
  ASSERT_TRUE(ice_controlling_attr != NULL);
  const StunByteStringAttribute* use_candidate_attr = msg->GetByteString(
      STUN_ATTR_USE_CANDIDATE);
  ASSERT_TRUE(use_candidate_attr != NULL);
}

TEST_F(PortTest, TestNetworkInfoAttribute) {
  std::unique_ptr<TestPort> lport(
      CreateTestPort(kLocalAddr1, "lfrag", "lpass"));
  // Set the network type for rport to be cellular so its cost will be 999.
  SetNetworkType(rtc::ADAPTER_TYPE_CELLULAR);
  std::unique_ptr<TestPort> rport(
      CreateTestPort(kLocalAddr2, "rfrag", "rpass"));
  lport->SetIceRole(cricket::ICEROLE_CONTROLLING);
  lport->SetIceTiebreaker(kTiebreaker1);
  rport->SetIceRole(cricket::ICEROLE_CONTROLLED);
  rport->SetIceTiebreaker(kTiebreaker2);

  uint16_t lnetwork_id = 9;
  lport->Network()->set_id(lnetwork_id);
  // Send a fake ping from lport to rport.
  lport->PrepareAddress();
  rport->PrepareAddress();
  Connection* lconn =
      lport->CreateConnection(rport->Candidates()[0], Port::ORIGIN_MESSAGE);
  lconn->Ping(0);
  ASSERT_TRUE_WAIT(lport->last_stun_msg() != NULL, 1000);
  IceMessage* msg = lport->last_stun_msg();
  const StunUInt32Attribute* network_info_attr =
      msg->GetUInt32(STUN_ATTR_NETWORK_INFO);
  ASSERT_TRUE(network_info_attr != NULL);
  uint32_t network_info = network_info_attr->value();
  EXPECT_EQ(lnetwork_id, network_info >> 16);
  // Default network cost is 0.
  EXPECT_EQ(0U, network_info & 0xFFFF);

  // Send a fake ping from rport to lport.
  uint16_t rnetwork_id = 8;
  rport->Network()->set_id(rnetwork_id);
  Connection* rconn =
      rport->CreateConnection(lport->Candidates()[0], Port::ORIGIN_MESSAGE);
  rconn->Ping(0);
  ASSERT_TRUE_WAIT(rport->last_stun_msg() != NULL, 1000);
  msg = rport->last_stun_msg();
  network_info_attr = msg->GetUInt32(STUN_ATTR_NETWORK_INFO);
  ASSERT_TRUE(network_info_attr != NULL);
  network_info = network_info_attr->value();
  EXPECT_EQ(rnetwork_id, network_info >> 16);
  EXPECT_EQ(cricket::kMaxNetworkCost, network_info & 0xFFFF);
}

// Test handling STUN messages.
TEST_F(PortTest, TestHandleStunMessage) {
  // Our port will act as the "remote" port.
  std::unique_ptr<TestPort> port(CreateTestPort(kLocalAddr2, "rfrag", "rpass"));

  std::unique_ptr<IceMessage> in_msg, out_msg;
  std::unique_ptr<ByteBufferWriter> buf(new ByteBufferWriter());
  rtc::SocketAddress addr(kLocalAddr1);
  std::string username;

  // BINDING-REQUEST from local to remote with valid ICE username,
  // MESSAGE-INTEGRITY, and FINGERPRINT.
  in_msg.reset(CreateStunMessageWithUsername(STUN_BINDING_REQUEST,
                                             "rfrag:lfrag"));
  in_msg->AddMessageIntegrity("rpass");
  in_msg->AddFingerprint();
  WriteStunMessage(in_msg.get(), buf.get());
  EXPECT_TRUE(port->GetStunMessage(buf->Data(), buf->Length(), addr, &out_msg,
                                   &username));
  EXPECT_TRUE(out_msg.get() != NULL);
  EXPECT_EQ("lfrag", username);

  // BINDING-RESPONSE without username, with MESSAGE-INTEGRITY and FINGERPRINT.
  in_msg.reset(CreateStunMessage(STUN_BINDING_RESPONSE));
  in_msg->AddAttribute(
      new StunXorAddressAttribute(STUN_ATTR_XOR_MAPPED_ADDRESS, kLocalAddr2));
  in_msg->AddMessageIntegrity("rpass");
  in_msg->AddFingerprint();
  WriteStunMessage(in_msg.get(), buf.get());
  EXPECT_TRUE(port->GetStunMessage(buf->Data(), buf->Length(), addr, &out_msg,
                                   &username));
  EXPECT_TRUE(out_msg.get() != NULL);
  EXPECT_EQ("", username);

  // BINDING-ERROR-RESPONSE without username, with error, M-I, and FINGERPRINT.
  in_msg.reset(CreateStunMessage(STUN_BINDING_ERROR_RESPONSE));
  in_msg->AddAttribute(new StunErrorCodeAttribute(STUN_ATTR_ERROR_CODE,
      STUN_ERROR_SERVER_ERROR, STUN_ERROR_REASON_SERVER_ERROR));
  in_msg->AddFingerprint();
  WriteStunMessage(in_msg.get(), buf.get());
  EXPECT_TRUE(port->GetStunMessage(buf->Data(), buf->Length(), addr, &out_msg,
                                   &username));
  EXPECT_TRUE(out_msg.get() != NULL);
  EXPECT_EQ("", username);
  ASSERT_TRUE(out_msg->GetErrorCode() != NULL);
  EXPECT_EQ(STUN_ERROR_SERVER_ERROR, out_msg->GetErrorCode()->code());
  EXPECT_EQ(std::string(STUN_ERROR_REASON_SERVER_ERROR),
      out_msg->GetErrorCode()->reason());
}

// Tests handling of ICE binding requests with missing or incorrect usernames.
TEST_F(PortTest, TestHandleStunMessageBadUsername) {
  std::unique_ptr<TestPort> port(CreateTestPort(kLocalAddr2, "rfrag", "rpass"));

  std::unique_ptr<IceMessage> in_msg, out_msg;
  std::unique_ptr<ByteBufferWriter> buf(new ByteBufferWriter());
  rtc::SocketAddress addr(kLocalAddr1);
  std::string username;

  // BINDING-REQUEST with no username.
  in_msg.reset(CreateStunMessage(STUN_BINDING_REQUEST));
  in_msg->AddMessageIntegrity("rpass");
  in_msg->AddFingerprint();
  WriteStunMessage(in_msg.get(), buf.get());
  EXPECT_TRUE(port->GetStunMessage(buf->Data(), buf->Length(), addr, &out_msg,
                                   &username));
  EXPECT_TRUE(out_msg.get() == NULL);
  EXPECT_EQ("", username);
  EXPECT_EQ(STUN_ERROR_BAD_REQUEST, port->last_stun_error_code());

  // BINDING-REQUEST with empty username.
  in_msg.reset(CreateStunMessageWithUsername(STUN_BINDING_REQUEST, ""));
  in_msg->AddMessageIntegrity("rpass");
  in_msg->AddFingerprint();
  WriteStunMessage(in_msg.get(), buf.get());
  EXPECT_TRUE(port->GetStunMessage(buf->Data(), buf->Length(), addr, &out_msg,
                                   &username));
  EXPECT_TRUE(out_msg.get() == NULL);
  EXPECT_EQ("", username);
  EXPECT_EQ(STUN_ERROR_UNAUTHORIZED, port->last_stun_error_code());

  // BINDING-REQUEST with too-short username.
  in_msg.reset(CreateStunMessageWithUsername(STUN_BINDING_REQUEST, "rfra"));
  in_msg->AddMessageIntegrity("rpass");
  in_msg->AddFingerprint();
  WriteStunMessage(in_msg.get(), buf.get());
  EXPECT_TRUE(port->GetStunMessage(buf->Data(), buf->Length(), addr, &out_msg,
                                   &username));
  EXPECT_TRUE(out_msg.get() == NULL);
  EXPECT_EQ("", username);
  EXPECT_EQ(STUN_ERROR_UNAUTHORIZED, port->last_stun_error_code());

  // BINDING-REQUEST with reversed username.
  in_msg.reset(CreateStunMessageWithUsername(STUN_BINDING_REQUEST,
                                            "lfrag:rfrag"));
  in_msg->AddMessageIntegrity("rpass");
  in_msg->AddFingerprint();
  WriteStunMessage(in_msg.get(), buf.get());
  EXPECT_TRUE(port->GetStunMessage(buf->Data(), buf->Length(), addr, &out_msg,
                                   &username));
  EXPECT_TRUE(out_msg.get() == NULL);
  EXPECT_EQ("", username);
  EXPECT_EQ(STUN_ERROR_UNAUTHORIZED, port->last_stun_error_code());

  // BINDING-REQUEST with garbage username.
  in_msg.reset(CreateStunMessageWithUsername(STUN_BINDING_REQUEST,
                                             "abcd:efgh"));
  in_msg->AddMessageIntegrity("rpass");
  in_msg->AddFingerprint();
  WriteStunMessage(in_msg.get(), buf.get());
  EXPECT_TRUE(port->GetStunMessage(buf->Data(), buf->Length(), addr, &out_msg,
                                   &username));
  EXPECT_TRUE(out_msg.get() == NULL);
  EXPECT_EQ("", username);
  EXPECT_EQ(STUN_ERROR_UNAUTHORIZED, port->last_stun_error_code());
}

// Test handling STUN messages with missing or malformed M-I.
TEST_F(PortTest, TestHandleStunMessageBadMessageIntegrity) {
  // Our port will act as the "remote" port.
  std::unique_ptr<TestPort> port(CreateTestPort(kLocalAddr2, "rfrag", "rpass"));

  std::unique_ptr<IceMessage> in_msg, out_msg;
  std::unique_ptr<ByteBufferWriter> buf(new ByteBufferWriter());
  rtc::SocketAddress addr(kLocalAddr1);
  std::string username;

  // BINDING-REQUEST from local to remote with valid ICE username and
  // FINGERPRINT, but no MESSAGE-INTEGRITY.
  in_msg.reset(CreateStunMessageWithUsername(STUN_BINDING_REQUEST,
                                             "rfrag:lfrag"));
  in_msg->AddFingerprint();
  WriteStunMessage(in_msg.get(), buf.get());
  EXPECT_TRUE(port->GetStunMessage(buf->Data(), buf->Length(), addr, &out_msg,
                                   &username));
  EXPECT_TRUE(out_msg.get() == NULL);
  EXPECT_EQ("", username);
  EXPECT_EQ(STUN_ERROR_BAD_REQUEST, port->last_stun_error_code());

  // BINDING-REQUEST from local to remote with valid ICE username and
  // FINGERPRINT, but invalid MESSAGE-INTEGRITY.
  in_msg.reset(CreateStunMessageWithUsername(STUN_BINDING_REQUEST,
                                             "rfrag:lfrag"));
  in_msg->AddMessageIntegrity("invalid");
  in_msg->AddFingerprint();
  WriteStunMessage(in_msg.get(), buf.get());
  EXPECT_TRUE(port->GetStunMessage(buf->Data(), buf->Length(), addr, &out_msg,
                                   &username));
  EXPECT_TRUE(out_msg.get() == NULL);
  EXPECT_EQ("", username);
  EXPECT_EQ(STUN_ERROR_UNAUTHORIZED, port->last_stun_error_code());

  // TODO: BINDING-RESPONSES and BINDING-ERROR-RESPONSES are checked
  // by the Connection, not the Port, since they require the remote username.
  // Change this test to pass in data via Connection::OnReadPacket instead.
}

// Test handling STUN messages with missing or malformed FINGERPRINT.
TEST_F(PortTest, TestHandleStunMessageBadFingerprint) {
  // Our port will act as the "remote" port.
  std::unique_ptr<TestPort> port(CreateTestPort(kLocalAddr2, "rfrag", "rpass"));

  std::unique_ptr<IceMessage> in_msg, out_msg;
  std::unique_ptr<ByteBufferWriter> buf(new ByteBufferWriter());
  rtc::SocketAddress addr(kLocalAddr1);
  std::string username;

  // BINDING-REQUEST from local to remote with valid ICE username and
  // MESSAGE-INTEGRITY, but no FINGERPRINT; GetStunMessage should fail.
  in_msg.reset(CreateStunMessageWithUsername(STUN_BINDING_REQUEST,
                                             "rfrag:lfrag"));
  in_msg->AddMessageIntegrity("rpass");
  WriteStunMessage(in_msg.get(), buf.get());
  EXPECT_FALSE(port->GetStunMessage(buf->Data(), buf->Length(), addr, &out_msg,
                                    &username));
  EXPECT_EQ(0, port->last_stun_error_code());

  // Now, add a fingerprint, but munge the message so it's not valid.
  in_msg->AddFingerprint();
  in_msg->SetTransactionID("TESTTESTBADD");
  WriteStunMessage(in_msg.get(), buf.get());
  EXPECT_FALSE(port->GetStunMessage(buf->Data(), buf->Length(), addr, &out_msg,
                                    &username));
  EXPECT_EQ(0, port->last_stun_error_code());

  // Valid BINDING-RESPONSE, except no FINGERPRINT.
  in_msg.reset(CreateStunMessage(STUN_BINDING_RESPONSE));
  in_msg->AddAttribute(
      new StunXorAddressAttribute(STUN_ATTR_XOR_MAPPED_ADDRESS, kLocalAddr2));
  in_msg->AddMessageIntegrity("rpass");
  WriteStunMessage(in_msg.get(), buf.get());
  EXPECT_FALSE(port->GetStunMessage(buf->Data(), buf->Length(), addr, &out_msg,
                                    &username));
  EXPECT_EQ(0, port->last_stun_error_code());

  // Now, add a fingerprint, but munge the message so it's not valid.
  in_msg->AddFingerprint();
  in_msg->SetTransactionID("TESTTESTBADD");
  WriteStunMessage(in_msg.get(), buf.get());
  EXPECT_FALSE(port->GetStunMessage(buf->Data(), buf->Length(), addr, &out_msg,
                                    &username));
  EXPECT_EQ(0, port->last_stun_error_code());

  // Valid BINDING-ERROR-RESPONSE, except no FINGERPRINT.
  in_msg.reset(CreateStunMessage(STUN_BINDING_ERROR_RESPONSE));
  in_msg->AddAttribute(new StunErrorCodeAttribute(STUN_ATTR_ERROR_CODE,
      STUN_ERROR_SERVER_ERROR, STUN_ERROR_REASON_SERVER_ERROR));
  in_msg->AddMessageIntegrity("rpass");
  WriteStunMessage(in_msg.get(), buf.get());
  EXPECT_FALSE(port->GetStunMessage(buf->Data(), buf->Length(), addr, &out_msg,
                                    &username));
  EXPECT_EQ(0, port->last_stun_error_code());

  // Now, add a fingerprint, but munge the message so it's not valid.
  in_msg->AddFingerprint();
  in_msg->SetTransactionID("TESTTESTBADD");
  WriteStunMessage(in_msg.get(), buf.get());
  EXPECT_FALSE(port->GetStunMessage(buf->Data(), buf->Length(), addr, &out_msg,
                                    &username));
  EXPECT_EQ(0, port->last_stun_error_code());
}

// Test handling of STUN binding indication messages . STUN binding
// indications are allowed only to the connection which is in read mode.
TEST_F(PortTest, TestHandleStunBindingIndication) {
  std::unique_ptr<TestPort> lport(
      CreateTestPort(kLocalAddr2, "lfrag", "lpass"));
  lport->SetIceRole(cricket::ICEROLE_CONTROLLING);
  lport->SetIceTiebreaker(kTiebreaker1);

  // Verifying encoding and decoding STUN indication message.
  std::unique_ptr<IceMessage> in_msg, out_msg;
  std::unique_ptr<ByteBufferWriter> buf(new ByteBufferWriter());
  rtc::SocketAddress addr(kLocalAddr1);
  std::string username;

  in_msg.reset(CreateStunMessage(STUN_BINDING_INDICATION));
  in_msg->AddFingerprint();
  WriteStunMessage(in_msg.get(), buf.get());
  EXPECT_TRUE(lport->GetStunMessage(buf->Data(), buf->Length(), addr, &out_msg,
                                    &username));
  EXPECT_TRUE(out_msg.get() != NULL);
  EXPECT_EQ(out_msg->type(), STUN_BINDING_INDICATION);
  EXPECT_EQ("", username);

  // Verify connection can handle STUN indication and updates
  // last_ping_received.
  std::unique_ptr<TestPort> rport(
      CreateTestPort(kLocalAddr2, "rfrag", "rpass"));
  rport->SetIceRole(cricket::ICEROLE_CONTROLLED);
  rport->SetIceTiebreaker(kTiebreaker2);

  lport->PrepareAddress();
  rport->PrepareAddress();
  ASSERT_FALSE(lport->Candidates().empty());
  ASSERT_FALSE(rport->Candidates().empty());

  Connection* lconn = lport->CreateConnection(rport->Candidates()[0],
                                              Port::ORIGIN_MESSAGE);
  Connection* rconn = rport->CreateConnection(lport->Candidates()[0],
                                              Port::ORIGIN_MESSAGE);
  rconn->Ping(0);

  ASSERT_TRUE_WAIT(rport->last_stun_msg() != NULL, 1000);
  IceMessage* msg = rport->last_stun_msg();
  EXPECT_EQ(STUN_BINDING_REQUEST, msg->type());
  // Send rport binding request to lport.
  lconn->OnReadPacket(rport->last_stun_buf()->data<char>(),
                      rport->last_stun_buf()->size(),
                      rtc::PacketTime());
  ASSERT_TRUE_WAIT(lport->last_stun_msg() != NULL, 1000);
  EXPECT_EQ(STUN_BINDING_RESPONSE, lport->last_stun_msg()->type());
  int64_t last_ping_received1 = lconn->last_ping_received();

  // Adding a delay of 100ms.
  rtc::Thread::Current()->ProcessMessages(100);
  // Pinging lconn using stun indication message.
  lconn->OnReadPacket(buf->Data(), buf->Length(), rtc::PacketTime());
  int64_t last_ping_received2 = lconn->last_ping_received();
  EXPECT_GT(last_ping_received2, last_ping_received1);
}

TEST_F(PortTest, TestComputeCandidatePriority) {
  std::unique_ptr<TestPort> port(CreateTestPort(kLocalAddr1, "name", "pass"));
  port->set_type_preference(90);
  port->set_component(177);
  port->AddCandidateAddress(SocketAddress("192.168.1.4", 1234));
  port->AddCandidateAddress(SocketAddress("2001:db8::1234", 1234));
  port->AddCandidateAddress(SocketAddress("fc12:3456::1234", 1234));
  port->AddCandidateAddress(SocketAddress("::ffff:192.168.1.4", 1234));
  port->AddCandidateAddress(SocketAddress("::192.168.1.4", 1234));
  port->AddCandidateAddress(SocketAddress("2002::1234:5678", 1234));
  port->AddCandidateAddress(SocketAddress("2001::1234:5678", 1234));
  port->AddCandidateAddress(SocketAddress("fecf::1234:5678", 1234));
  port->AddCandidateAddress(SocketAddress("3ffe::1234:5678", 1234));
  // These should all be:
  // (90 << 24) | ([rfc3484 pref value] << 8) | (256 - 177)
  uint32_t expected_priority_v4 = 1509957199U;
  uint32_t expected_priority_v6 = 1509959759U;
  uint32_t expected_priority_ula = 1509962319U;
  uint32_t expected_priority_v4mapped = expected_priority_v4;
  uint32_t expected_priority_v4compat = 1509949775U;
  uint32_t expected_priority_6to4 = 1509954639U;
  uint32_t expected_priority_teredo = 1509952079U;
  uint32_t expected_priority_sitelocal = 1509949775U;
  uint32_t expected_priority_6bone = 1509949775U;
  ASSERT_EQ(expected_priority_v4, port->Candidates()[0].priority());
  ASSERT_EQ(expected_priority_v6, port->Candidates()[1].priority());
  ASSERT_EQ(expected_priority_ula, port->Candidates()[2].priority());
  ASSERT_EQ(expected_priority_v4mapped, port->Candidates()[3].priority());
  ASSERT_EQ(expected_priority_v4compat, port->Candidates()[4].priority());
  ASSERT_EQ(expected_priority_6to4, port->Candidates()[5].priority());
  ASSERT_EQ(expected_priority_teredo, port->Candidates()[6].priority());
  ASSERT_EQ(expected_priority_sitelocal, port->Candidates()[7].priority());
  ASSERT_EQ(expected_priority_6bone, port->Candidates()[8].priority());
}

// In the case of shared socket, one port may be shared by local and stun.
// Test that candidates with different types will have different foundation.
TEST_F(PortTest, TestFoundation) {
  std::unique_ptr<TestPort> testport(
      CreateTestPort(kLocalAddr1, "name", "pass"));
  testport->AddCandidateAddress(kLocalAddr1, kLocalAddr1,
                                LOCAL_PORT_TYPE,
                                cricket::ICE_TYPE_PREFERENCE_HOST, false);
  testport->AddCandidateAddress(kLocalAddr2, kLocalAddr1,
                                STUN_PORT_TYPE,
                                cricket::ICE_TYPE_PREFERENCE_SRFLX, true);
  EXPECT_NE(testport->Candidates()[0].foundation(),
            testport->Candidates()[1].foundation());
}

// This test verifies the foundation of different types of ICE candidates.
TEST_F(PortTest, TestCandidateFoundation) {
  std::unique_ptr<rtc::NATServer> nat_server(
      CreateNatServer(kNatAddr1, NAT_OPEN_CONE));
  std::unique_ptr<UDPPort> udpport1(CreateUdpPort(kLocalAddr1));
  udpport1->PrepareAddress();
  std::unique_ptr<UDPPort> udpport2(CreateUdpPort(kLocalAddr1));
  udpport2->PrepareAddress();
  EXPECT_EQ(udpport1->Candidates()[0].foundation(),
            udpport2->Candidates()[0].foundation());
  std::unique_ptr<TCPPort> tcpport1(CreateTcpPort(kLocalAddr1));
  tcpport1->PrepareAddress();
  std::unique_ptr<TCPPort> tcpport2(CreateTcpPort(kLocalAddr1));
  tcpport2->PrepareAddress();
  EXPECT_EQ(tcpport1->Candidates()[0].foundation(),
            tcpport2->Candidates()[0].foundation());
  std::unique_ptr<Port> stunport(
      CreateStunPort(kLocalAddr1, nat_socket_factory1()));
  stunport->PrepareAddress();
  ASSERT_EQ_WAIT(1U, stunport->Candidates().size(), kTimeout);
  EXPECT_NE(tcpport1->Candidates()[0].foundation(),
            stunport->Candidates()[0].foundation());
  EXPECT_NE(tcpport2->Candidates()[0].foundation(),
            stunport->Candidates()[0].foundation());
  EXPECT_NE(udpport1->Candidates()[0].foundation(),
            stunport->Candidates()[0].foundation());
  EXPECT_NE(udpport2->Candidates()[0].foundation(),
            stunport->Candidates()[0].foundation());
  // Verify GTURN candidate foundation.
  std::unique_ptr<RelayPort> relayport(CreateGturnPort(kLocalAddr1));
  relayport->AddServerAddress(
      cricket::ProtocolAddress(kRelayUdpIntAddr, cricket::PROTO_UDP));
  relayport->PrepareAddress();
  ASSERT_EQ_WAIT(1U, relayport->Candidates().size(), kTimeout);
  EXPECT_NE(udpport1->Candidates()[0].foundation(),
            relayport->Candidates()[0].foundation());
  EXPECT_NE(udpport2->Candidates()[0].foundation(),
            relayport->Candidates()[0].foundation());
  // Verifying TURN candidate foundation.
  std::unique_ptr<Port> turnport1(
      CreateTurnPort(kLocalAddr1, nat_socket_factory1(), PROTO_UDP, PROTO_UDP));
  turnport1->PrepareAddress();
  ASSERT_EQ_WAIT(1U, turnport1->Candidates().size(), kTimeout);
  EXPECT_NE(udpport1->Candidates()[0].foundation(),
            turnport1->Candidates()[0].foundation());
  EXPECT_NE(udpport2->Candidates()[0].foundation(),
            turnport1->Candidates()[0].foundation());
  EXPECT_NE(stunport->Candidates()[0].foundation(),
            turnport1->Candidates()[0].foundation());
  std::unique_ptr<Port> turnport2(
      CreateTurnPort(kLocalAddr1, nat_socket_factory1(), PROTO_UDP, PROTO_UDP));
  turnport2->PrepareAddress();
  ASSERT_EQ_WAIT(1U, turnport2->Candidates().size(), kTimeout);
  EXPECT_EQ(turnport1->Candidates()[0].foundation(),
            turnport2->Candidates()[0].foundation());

  // Running a second turn server, to get different base IP address.
  SocketAddress kTurnUdpIntAddr2("99.99.98.4", STUN_SERVER_PORT);
  SocketAddress kTurnUdpExtAddr2("99.99.98.5", 0);
  TestTurnServer turn_server2(
      rtc::Thread::Current(), kTurnUdpIntAddr2, kTurnUdpExtAddr2);
  std::unique_ptr<Port> turnport3(
      CreateTurnPort(kLocalAddr1, nat_socket_factory1(), PROTO_UDP, PROTO_UDP,
                     kTurnUdpIntAddr2));
  turnport3->PrepareAddress();
  ASSERT_EQ_WAIT(1U, turnport3->Candidates().size(), kTimeout);
  EXPECT_NE(turnport3->Candidates()[0].foundation(),
            turnport2->Candidates()[0].foundation());

  // Start a TCP turn server, and check that two turn candidates have
  // different foundations if their relay protocols are different.
  TestTurnServer turn_server3(rtc::Thread::Current(), kTurnTcpIntAddr,
                              kTurnUdpExtAddr, PROTO_TCP);
  std::unique_ptr<Port> turnport4(
      CreateTurnPort(kLocalAddr1, nat_socket_factory1(), PROTO_TCP, PROTO_UDP));
  turnport4->PrepareAddress();
  ASSERT_EQ_WAIT(1U, turnport4->Candidates().size(), kTimeout);
  EXPECT_NE(turnport2->Candidates()[0].foundation(),
            turnport4->Candidates()[0].foundation());
}

// This test verifies the related addresses of different types of
// ICE candiates.
TEST_F(PortTest, TestCandidateRelatedAddress) {
  std::unique_ptr<rtc::NATServer> nat_server(
      CreateNatServer(kNatAddr1, NAT_OPEN_CONE));
  std::unique_ptr<UDPPort> udpport(CreateUdpPort(kLocalAddr1));
  udpport->PrepareAddress();
  // For UDPPort, related address will be empty.
  EXPECT_TRUE(udpport->Candidates()[0].related_address().IsNil());
  // Testing related address for stun candidates.
  // For stun candidate related address must be equal to the base
  // socket address.
  std::unique_ptr<StunPort> stunport(
      CreateStunPort(kLocalAddr1, nat_socket_factory1()));
  stunport->PrepareAddress();
  ASSERT_EQ_WAIT(1U, stunport->Candidates().size(), kTimeout);
  // Check STUN candidate address.
  EXPECT_EQ(stunport->Candidates()[0].address().ipaddr(),
            kNatAddr1.ipaddr());
  // Check STUN candidate related address.
  EXPECT_EQ(stunport->Candidates()[0].related_address(),
            stunport->GetLocalAddress());
  // Verifying the related address for the GTURN candidates.
  // NOTE: In case of GTURN related address will be equal to the mapped
  // address, but address(mapped) will not be XOR.
  std::unique_ptr<RelayPort> relayport(CreateGturnPort(kLocalAddr1));
  relayport->AddServerAddress(
      cricket::ProtocolAddress(kRelayUdpIntAddr, cricket::PROTO_UDP));
  relayport->PrepareAddress();
  ASSERT_EQ_WAIT(1U, relayport->Candidates().size(), kTimeout);
  // For Gturn related address is set to "0.0.0.0:0"
  EXPECT_EQ(rtc::SocketAddress(),
            relayport->Candidates()[0].related_address());
  // Verifying the related address for TURN candidate.
  // For TURN related address must be equal to the mapped address.
  std::unique_ptr<Port> turnport(
      CreateTurnPort(kLocalAddr1, nat_socket_factory1(), PROTO_UDP, PROTO_UDP));
  turnport->PrepareAddress();
  ASSERT_EQ_WAIT(1U, turnport->Candidates().size(), kTimeout);
  EXPECT_EQ(kTurnUdpExtAddr.ipaddr(),
            turnport->Candidates()[0].address().ipaddr());
  EXPECT_EQ(kNatAddr1.ipaddr(),
            turnport->Candidates()[0].related_address().ipaddr());
}

// Test priority value overflow handling when preference is set to 3.
TEST_F(PortTest, TestCandidatePriority) {
  cricket::Candidate cand1;
  cand1.set_priority(3);
  cricket::Candidate cand2;
  cand2.set_priority(1);
  EXPECT_TRUE(cand1.priority() > cand2.priority());
}

// Test the Connection priority is calculated correctly.
TEST_F(PortTest, TestConnectionPriority) {
  std::unique_ptr<TestPort> lport(
      CreateTestPort(kLocalAddr1, "lfrag", "lpass"));
  lport->set_type_preference(cricket::ICE_TYPE_PREFERENCE_HOST);
  std::unique_ptr<TestPort> rport(
      CreateTestPort(kLocalAddr2, "rfrag", "rpass"));
  rport->set_type_preference(cricket::ICE_TYPE_PREFERENCE_RELAY);
  lport->set_component(123);
  lport->AddCandidateAddress(SocketAddress("192.168.1.4", 1234));
  rport->set_component(23);
  rport->AddCandidateAddress(SocketAddress("10.1.1.100", 1234));

  EXPECT_EQ(0x7E001E85U, lport->Candidates()[0].priority());
  EXPECT_EQ(0x2001EE9U, rport->Candidates()[0].priority());

  // RFC 5245
  // pair priority = 2^32*MIN(G,D) + 2*MAX(G,D) + (G>D?1:0)
  lport->SetIceRole(cricket::ICEROLE_CONTROLLING);
  rport->SetIceRole(cricket::ICEROLE_CONTROLLED);
  Connection* lconn = lport->CreateConnection(
      rport->Candidates()[0], Port::ORIGIN_MESSAGE);
#if defined(WEBRTC_WIN)
  EXPECT_EQ(0x2001EE9FC003D0BU, lconn->priority());
#else
  EXPECT_EQ(0x2001EE9FC003D0BLLU, lconn->priority());
#endif

  lport->SetIceRole(cricket::ICEROLE_CONTROLLED);
  rport->SetIceRole(cricket::ICEROLE_CONTROLLING);
  Connection* rconn = rport->CreateConnection(
      lport->Candidates()[0], Port::ORIGIN_MESSAGE);
#if defined(WEBRTC_WIN)
  EXPECT_EQ(0x2001EE9FC003D0AU, rconn->priority());
#else
  EXPECT_EQ(0x2001EE9FC003D0ALLU, rconn->priority());
#endif
}

TEST_F(PortTest, TestWritableState) {
  UDPPort* port1 = CreateUdpPort(kLocalAddr1);
  port1->SetIceRole(cricket::ICEROLE_CONTROLLING);
  UDPPort* port2 = CreateUdpPort(kLocalAddr2);
  port2->SetIceRole(cricket::ICEROLE_CONTROLLED);

  // Set up channels.
  TestChannel ch1(port1);
  TestChannel ch2(port2);

  // Acquire addresses.
  ch1.Start();
  ch2.Start();
  ASSERT_EQ_WAIT(1, ch1.complete_count(), kTimeout);
  ASSERT_EQ_WAIT(1, ch2.complete_count(), kTimeout);

  // Send a ping from src to dst.
  ch1.CreateConnection(GetCandidate(port2));
  ASSERT_TRUE(ch1.conn() != NULL);
  EXPECT_EQ(Connection::STATE_WRITE_INIT, ch1.conn()->write_state());
  EXPECT_TRUE_WAIT(ch1.conn()->connected(), kTimeout);  // for TCP connect
  ch1.Ping();
  WAIT(!ch2.remote_address().IsNil(), kTimeout);

  // Data should be unsendable until the connection is accepted.
  char data[] = "abcd";
  int data_size = arraysize(data);
  rtc::PacketOptions options;
  EXPECT_EQ(SOCKET_ERROR, ch1.conn()->Send(data, data_size, options));

  // Accept the connection to return the binding response, transition to
  // writable, and allow data to be sent.
  ch2.AcceptConnection(GetCandidate(port1));
  EXPECT_EQ_WAIT(Connection::STATE_WRITABLE, ch1.conn()->write_state(),
                 kTimeout);
  EXPECT_EQ(data_size, ch1.conn()->Send(data, data_size, options));

  // Ask the connection to update state as if enough time has passed to lose
  // full writability and 5 pings went unresponded to. We'll accomplish the
  // latter by sending pings but not pumping messages.
  for (uint32_t i = 1; i <= CONNECTION_WRITE_CONNECT_FAILURES; ++i) {
    ch1.Ping(i);
  }
  int unreliable_timeout_delay = CONNECTION_WRITE_CONNECT_TIMEOUT + 500;
  ch1.conn()->UpdateState(unreliable_timeout_delay);
  EXPECT_EQ(Connection::STATE_WRITE_UNRELIABLE, ch1.conn()->write_state());

  // Data should be able to be sent in this state.
  EXPECT_EQ(data_size, ch1.conn()->Send(data, data_size, options));

  // And now allow the other side to process the pings and send binding
  // responses.
  EXPECT_EQ_WAIT(Connection::STATE_WRITABLE, ch1.conn()->write_state(),
                 kTimeout);

  // Wait long enough for a full timeout (past however long we've already
  // waited).
  for (uint32_t i = 1; i <= CONNECTION_WRITE_CONNECT_FAILURES; ++i) {
    ch1.Ping(unreliable_timeout_delay + i);
  }
  ch1.conn()->UpdateState(unreliable_timeout_delay + CONNECTION_WRITE_TIMEOUT +
                          500u);
  EXPECT_EQ(Connection::STATE_WRITE_TIMEOUT, ch1.conn()->write_state());

  // Now that the connection has completely timed out, data send should fail.
  EXPECT_EQ(SOCKET_ERROR, ch1.conn()->Send(data, data_size, options));

  ch1.Stop();
  ch2.Stop();
}

TEST_F(PortTest, TestTimeoutForNeverWritable) {
  UDPPort* port1 = CreateUdpPort(kLocalAddr1);
  port1->SetIceRole(cricket::ICEROLE_CONTROLLING);
  UDPPort* port2 = CreateUdpPort(kLocalAddr2);
  port2->SetIceRole(cricket::ICEROLE_CONTROLLED);

  // Set up channels.
  TestChannel ch1(port1);
  TestChannel ch2(port2);

  // Acquire addresses.
  ch1.Start();
  ch2.Start();

  ch1.CreateConnection(GetCandidate(port2));
  ASSERT_TRUE(ch1.conn() != NULL);
  EXPECT_EQ(Connection::STATE_WRITE_INIT, ch1.conn()->write_state());

  // Attempt to go directly to write timeout.
  for (uint32_t i = 1; i <= CONNECTION_WRITE_CONNECT_FAILURES; ++i) {
    ch1.Ping(i);
  }
  ch1.conn()->UpdateState(CONNECTION_WRITE_TIMEOUT + 500u);
  EXPECT_EQ(Connection::STATE_WRITE_TIMEOUT, ch1.conn()->write_state());
}

// This test verifies the connection setup between ICEMODE_FULL
// and ICEMODE_LITE.
// In this test |ch1| behaves like FULL mode client and we have created
// port which responds to the ping message just like LITE client.
TEST_F(PortTest, TestIceLiteConnectivity) {
  TestPort* ice_full_port = CreateTestPort(
      kLocalAddr1, "lfrag", "lpass",
      cricket::ICEROLE_CONTROLLING, kTiebreaker1);

  std::unique_ptr<TestPort> ice_lite_port(
      CreateTestPort(kLocalAddr2, "rfrag", "rpass", cricket::ICEROLE_CONTROLLED,
                     kTiebreaker2));
  // Setup TestChannel. This behaves like FULL mode client.
  TestChannel ch1(ice_full_port);
  ch1.SetIceMode(ICEMODE_FULL);

  // Start gathering candidates.
  ch1.Start();
  ice_lite_port->PrepareAddress();

  ASSERT_EQ_WAIT(1, ch1.complete_count(), kTimeout);
  ASSERT_FALSE(ice_lite_port->Candidates().empty());

  ch1.CreateConnection(GetCandidate(ice_lite_port.get()));
  ASSERT_TRUE(ch1.conn() != NULL);
  EXPECT_EQ(Connection::STATE_WRITE_INIT, ch1.conn()->write_state());

  // Send ping from full mode client.
  // This ping must not have USE_CANDIDATE_ATTR.
  ch1.Ping();

  // Verify stun ping is without USE_CANDIDATE_ATTR. Getting message directly
  // from port.
  ASSERT_TRUE_WAIT(ice_full_port->last_stun_msg() != NULL, 1000);
  IceMessage* msg = ice_full_port->last_stun_msg();
  EXPECT_TRUE(msg->GetByteString(STUN_ATTR_USE_CANDIDATE) == NULL);

  // Respond with a BINDING-RESPONSE from litemode client.
  // NOTE: Ideally we should't create connection at this stage from lite
  // port, as it should be done only after receiving ping with USE_CANDIDATE.
  // But we need a connection to send a response message.
  ice_lite_port->CreateConnection(
      ice_full_port->Candidates()[0], cricket::Port::ORIGIN_MESSAGE);
  std::unique_ptr<IceMessage> request(CopyStunMessage(msg));
  ice_lite_port->SendBindingResponse(
      request.get(), ice_full_port->Candidates()[0].address());

  // Feeding the respone message from litemode to the full mode connection.
  ch1.conn()->OnReadPacket(ice_lite_port->last_stun_buf()->data<char>(),
                           ice_lite_port->last_stun_buf()->size(),
                           rtc::PacketTime());
  // Verifying full mode connection becomes writable from the response.
  EXPECT_EQ_WAIT(Connection::STATE_WRITABLE, ch1.conn()->write_state(),
                 kTimeout);
  EXPECT_TRUE_WAIT(ch1.nominated(), kTimeout);

  // Clear existing stun messsages. Otherwise we will process old stun
  // message right after we send ping.
  ice_full_port->Reset();
  // Send ping. This must have USE_CANDIDATE_ATTR.
  ch1.Ping();
  ASSERT_TRUE_WAIT(ice_full_port->last_stun_msg() != NULL, 1000);
  msg = ice_full_port->last_stun_msg();
  EXPECT_TRUE(msg->GetByteString(STUN_ATTR_USE_CANDIDATE) != NULL);
  ch1.Stop();
}

// This test case verifies that the CONTROLLING port does not time out.
TEST_F(PortTest, TestControllingNoTimeout) {
  UDPPort* port1 = CreateUdpPort(kLocalAddr1);
  ConnectToSignalDestroyed(port1);
  port1->set_timeout_delay(10);  // milliseconds
  port1->SetIceRole(cricket::ICEROLE_CONTROLLING);
  port1->SetIceTiebreaker(kTiebreaker1);

  UDPPort* port2 = CreateUdpPort(kLocalAddr2);
  port2->SetIceRole(cricket::ICEROLE_CONTROLLED);
  port2->SetIceTiebreaker(kTiebreaker2);

  // Set up channels and ensure both ports will be deleted.
  TestChannel ch1(port1);
  TestChannel ch2(port2);

  // Simulate a connection that succeeds, and then is destroyed.
  StartConnectAndStopChannels(&ch1, &ch2);

  // After the connection is destroyed, the port should not be destroyed.
  rtc::Thread::Current()->ProcessMessages(kTimeout);
  EXPECT_FALSE(destroyed());
}

// This test case verifies that the CONTROLLED port does time out, but only
// after connectivity is lost.
TEST_F(PortTest, TestControlledTimeout) {
  UDPPort* port1 = CreateUdpPort(kLocalAddr1);
  port1->SetIceRole(cricket::ICEROLE_CONTROLLING);
  port1->SetIceTiebreaker(kTiebreaker1);

  UDPPort* port2 = CreateUdpPort(kLocalAddr2);
  ConnectToSignalDestroyed(port2);
  port2->set_timeout_delay(10);  // milliseconds
  port2->SetIceRole(cricket::ICEROLE_CONTROLLED);
  port2->SetIceTiebreaker(kTiebreaker2);

  // The connection must not be destroyed before a connection is attempted.
  EXPECT_FALSE(destroyed());

  port1->set_component(cricket::ICE_CANDIDATE_COMPONENT_DEFAULT);
  port2->set_component(cricket::ICE_CANDIDATE_COMPONENT_DEFAULT);

  // Set up channels and ensure both ports will be deleted.
  TestChannel ch1(port1);
  TestChannel ch2(port2);

  // Simulate a connection that succeeds, and then is destroyed.
  StartConnectAndStopChannels(&ch1, &ch2);

  // The controlled port should be destroyed after 10 milliseconds.
  EXPECT_TRUE_WAIT(destroyed(), kTimeout);
}

// This test case verifies that if the role of a port changes from controlled
// to controlling after all connections fail, the port will not be destroyed.
TEST_F(PortTest, TestControlledToControllingNotDestroyed) {
  UDPPort* port1 = CreateUdpPort(kLocalAddr1);
  port1->SetIceRole(cricket::ICEROLE_CONTROLLING);
  port1->SetIceTiebreaker(kTiebreaker1);

  UDPPort* port2 = CreateUdpPort(kLocalAddr2);
  ConnectToSignalDestroyed(port2);
  port2->set_timeout_delay(10);  // milliseconds
  port2->SetIceRole(cricket::ICEROLE_CONTROLLED);
  port2->SetIceTiebreaker(kTiebreaker2);

  // The connection must not be destroyed before a connection is attempted.
  EXPECT_FALSE(destroyed());

  port1->set_component(cricket::ICE_CANDIDATE_COMPONENT_DEFAULT);
  port2->set_component(cricket::ICE_CANDIDATE_COMPONENT_DEFAULT);

  // Set up channels and ensure both ports will be deleted.
  TestChannel ch1(port1);
  TestChannel ch2(port2);

  // Simulate a connection that succeeds, and then is destroyed.
  StartConnectAndStopChannels(&ch1, &ch2);
  // Switch the role after all connections are destroyed.
  EXPECT_TRUE_WAIT(ch2.conn() == nullptr, kTimeout);
  port1->SetIceRole(cricket::ICEROLE_CONTROLLED);
  port2->SetIceRole(cricket::ICEROLE_CONTROLLING);

  // After the connection is destroyed, the port should not be destroyed.
  rtc::Thread::Current()->ProcessMessages(kTimeout);
  EXPECT_FALSE(destroyed());
}

TEST_F(PortTest, TestSupportsProtocol) {
  std::unique_ptr<Port> udp_port(CreateUdpPort(kLocalAddr1));
  EXPECT_TRUE(udp_port->SupportsProtocol(UDP_PROTOCOL_NAME));
  EXPECT_FALSE(udp_port->SupportsProtocol(TCP_PROTOCOL_NAME));

  std::unique_ptr<Port> stun_port(
      CreateStunPort(kLocalAddr1, nat_socket_factory1()));
  EXPECT_TRUE(stun_port->SupportsProtocol(UDP_PROTOCOL_NAME));
  EXPECT_FALSE(stun_port->SupportsProtocol(TCP_PROTOCOL_NAME));

  std::unique_ptr<Port> tcp_port(CreateTcpPort(kLocalAddr1));
  EXPECT_TRUE(tcp_port->SupportsProtocol(TCP_PROTOCOL_NAME));
  EXPECT_TRUE(tcp_port->SupportsProtocol(SSLTCP_PROTOCOL_NAME));
  EXPECT_FALSE(tcp_port->SupportsProtocol(UDP_PROTOCOL_NAME));

  std::unique_ptr<Port> turn_port(
      CreateTurnPort(kLocalAddr1, nat_socket_factory1(), PROTO_UDP, PROTO_UDP));
  EXPECT_TRUE(turn_port->SupportsProtocol(UDP_PROTOCOL_NAME));
  EXPECT_FALSE(turn_port->SupportsProtocol(TCP_PROTOCOL_NAME));
}
