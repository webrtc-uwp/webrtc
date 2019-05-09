#include "remoting/connection.h"
#include "remoting/config.h"
#include "test/gtest.h"

using namespace hololight::remoting;

// test fixture sample. add constructor/SetUp method to initialize connection
class ConnectionTest : public ::testing::Test {
 public:
  ConnectionTest() { connection = std::make_unique<Connection>(); }

 protected:
  std::unique_ptr<Connection> connection;
};

TEST_F(ConnectionTest, InitServer) {
  ASSERT_TRUE(connection.get() != nullptr);

  auto remoting_config = Config::default_server_test();

  // this is problematic because we kinda need to mock d3d behavior (or use a
  // fake video source somewhere)
  auto gfx_config = GraphicsApiConfig(nullptr, nullptr, nullptr);
  auto result = connection->Init(remoting_config, gfx_config);

  ASSERT_TRUE(result.has_value());
}

TEST_F(ConnectionTest, InitClientFailsWithInvalidDecoder) {
  auto remoting_config = Config::default_client();

  // actually, I don't think webrtc needs d3d on the client side, so it should
  // use absl::optional
  auto gfx_config = GraphicsApiConfig(nullptr, nullptr, nullptr);
  auto result = connection->Init(remoting_config, gfx_config);

  ASSERT_FALSE(result.has_value());
  ASSERT_EQ(result.error().type, Error::Type::UnsupportedConfig);
}

TEST_F(ConnectionTest, ServerConnect) {
  ASSERT_TRUE(connection.get() != nullptr);

  auto remoting_config = Config::default_server_test();

  // this is problematic because we kinda need to mock d3d behavior (or use a
  // fake video source somewhere)
  auto gfx_config = GraphicsApiConfig(nullptr, nullptr, nullptr);
  auto result = connection->Init(remoting_config, gfx_config);

  ASSERT_TRUE(result.has_value());

  connection->Connect(remoting_config);
}

// this kinda didn't work out as expected, this whole friend class fuckery makes
// it harder than it should be. But how do we test shit? D3D should be kinda
// limited to an integration test, or maybe I should test with a fake source or
// something.
// namespace hololight::remoting {
// class ConnectionTest : public ::testing::Test {
//  protected:
//  rtc::scoped_refptr<Connection> connection;
// };

// TEST_F(ConnectionTest, VideoEncoderFactoryTest) {
//     auto config = Config::default_client();
//     connection->CreateVideoEncoderFactory(config);
// }
//}  // namespace hololight::remoting

// we could try making a test that accesses a private method