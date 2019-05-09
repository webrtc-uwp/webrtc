#include "remoting_api.h"

#include "signaling/factories/signaling_factory.h"
#include "remoting/factories/client_to_server_connection_factory.h"
#include "remoting/factories/server_to_client_connection_factory.h"
#include "remoting/client_to_server_connection.h"
#include "remoting/server_to_client_connection.h"

#include <string>
#include <optional>
#include <cassert>
#include <iostream>
#include <mutex>

// Markus: We need this import for now because poses come as JSON
// We will remove this and migrate to a binary format
#include "third_party/jsoncpp/source/include/json/json.h"
const char* const KEY_POSE_VIEW_LEFT = "viewLeft";
const char* const KEY_POSE_VIEW_RIGHT = "viewRight";
const char* const KEY_POSE_PROJ_LEFT = "projLeft";
const char* const KEY_POSE_PROJ_RIGHT = "projRight";
const char* const KEY_POSE_CAM_POS = "camPos";
const char* const KEY_POSE_CAM_ROT = "camRot";

std::string SerializePoseAsJson(XrStereoPose& pose);
XrStereoPose DecodePoseFromJson(std::string& poseJson);

// static std::map<uint32_t, std::unique_ptr<hololight::remoting::Connection>>
//     g_connection_map;
// static uint32_t next_index = 0;

// uint32_t GetNextIndex() {
//   // this overflows on 2^32 connections in one process, but we expect slightly
//   // less, so it's ok
//   return next_index++;
// }

// void Init(const char* config_path) {
//    auto connection = std::make_unique<hololight::remoting::Connection>();

//   hololight::remoting::GraphicsApiConfig gfx_cfg{nullptr, nullptr, nullptr};
//   auto result = connection->Init(config_path, gfx_cfg);

//   if (result.has_value()) {
//     uint32_t index = GetNextIndex();
//     g_connection_map[index] = std::move(connection);
//   }
// }

static std::mutex g_connectLock;

static FailureCallback g_failureCallback = nullptr;

static std::unique_ptr<hololight::remoting::server_to_client_connection> g_toClientConn = nullptr;
static std::unique_ptr<hololight::remoting::client_to_server_connection> g_toServerConn = nullptr;

void Init(FailureCallback onFailure) {

  assert(onFailure && "Failure callback must not be nullptr");
  g_failureCallback = onFailure;
}

REMOTING_HANDLE ConnectToServerTcp(const char * const ip, uint16_t port, ClientConfig clientConfig) {

  std::lock_guard<std::mutex> lock(g_connectLock);

  assert(g_failureCallback && "A failure callback must be set by calling Init first");
  assert(!g_toClientConn && !g_toServerConn && "Cannot create a new connection while another connection is still active");

  try
  {
    // Create relay
    hololight::signaling::factories::SignalingFactory fSig;
    auto relay = fSig.create_tcp_relay_from_connect(
      std::string(ip),
      port
    );

    // Create P2P connection
    hololight::remoting::factories::client_to_server_connection_factory fConn(std::move(relay));
    g_toServerConn = fConn.create_connection();

    // TODO: Think of a smarter handle
    return (REMOTING_HANDLE)(uint64_t)g_toServerConn.get();
  }
  catch(const std::exception& e)
  {
    std::cerr << e.what() << '\n';
    assert(false && "Uncaught internal error");
  }
  catch (...) {
     std::cerr << "Weird error. Call cops." << '\n';
  }
}

REMOTING_HANDLE ListenForClientTcp(uint16_t port, ServerConfig serverConfig) {

  std::lock_guard<std::mutex> lock(g_connectLock);

  assert(g_failureCallback && "A failure callback must be set by calling Init first");
  assert(!g_toClientConn && !g_toServerConn && "Cannot create a new connection while another connection is still active");

  try
  {
    // Create relay
    hololight::signaling::factories::SignalingFactory fSig;
    auto relay = fSig.create_tcp_relay_from_listen(port);

    // Create P2P connection
    hololight::remoting::factories::server_to_client_connection_factory fConn(std::move(relay));

    // Markus:
    // TODO: This API should take the generic GraphicsApiConfig class, not the platform-specific components
    #if defined(WEBRTC_WIN)
    g_toClientConn = fConn.create_connection(
      serverConfig.graphicsApiConfig.d3d_device, 
      serverConfig.graphicsApiConfig.d3d_context, 
      *serverConfig.graphicsApiConfig.render_target_desc);
    #else
    #error "Unsupported platform"
    #endif

    // TODO: Think of a smarter handle
    return (REMOTING_HANDLE)(uint64_t)g_toClientConn.get();
  }
  catch(const std::exception& e)
  {
    std::cerr << e.what() << '\n';
    assert(false && "Uncaught internal error");
  }
  catch (...) {
     std::cerr << "Weird error. Call cops." << '\n';
  }
}

void CloseConnection(REMOTING_HANDLE hConn) {

  std::lock_guard<std::mutex> lock(g_connectLock);

  assert((g_toClientConn || g_toServerConn) && "Cannot close a connection that is not active");

  if (g_toServerConn)
    g_toServerConn = nullptr;

  if (g_toClientConn)
    g_toClientConn = nullptr;
}

bool TryRunWithNextPose(REMOTING_HANDLE hConn, PoseCallback poseCallback, void* userData) {

  assert(g_toClientConn && "Intitate connection to client before calling this");

  auto pose = g_toClientConn->poll_next_input(0);

  if (pose) {

    // Markus:
    // TODO: This translation should not have to happen here, but should instead happen inside poll_next_input
    XrStereoPose pose4real = DecodePoseFromJson(pose.value());

    poseCallback(&pose4real, userData);

    return true;
  } else {
    return false;
  }
}

void PushFrameAsync(REMOTING_HANDLE hConn, GraphicsApiFrame frame) {

  assert(g_toClientConn && "Intitate connection to client before calling this");

// Markus:
// TODO: Send frame should take the "generic" GraphicsApiFrame instead of having these weird overloads
// We should pass GraphicsApiFrame down as far as possible before caring about platform differences
#if defined(WEBRTC_WIN)
  g_toClientConn->send_frame(frame.d3d_frame);
#else
  assert(false && "Unsupported platform");
#endif
}

bool TryRunWithNextFrame(REMOTING_HANDLE hConn, FrameCallback frameCallback, void* userData) {

  assert(g_toServerConn && "Intitate connection to server before calling this");

  return g_toServerConn->try_exec_with_frame([&](hololight::remoting::video::video_frame* frame) {

    // Markus:
    // TODO: This is the most disgusting unsafe cast in here, but I still prefer it over
    // exposing the internal video_frame struct.
    RawFrame rawFrame = *(RawFrame*)frame;

    frameCallback(rawFrame, userData);
  });
}

void PushPoseAsync(REMOTING_HANDLE hConn, XrStereoPose* pose) {

  assert(g_toServerConn && "Intitate connection to server before calling this");

  // Markus:
  // TODO: We need to get rid of this conversion here and make send_input take a XrStereoPose directly
  std::string poseJson = SerializePoseAsJson(*pose);

  g_toServerConn->send_input(poseJson);
}

// Markus: TODO: Get rid of these functions below once we switch to a binary format
Matrix4x4 DeserializeMatrixFromJson(Json::Value& json) {
  Matrix4x4 m;

  m._11 = json[0].asFloat();
  m._12 = json[1].asFloat();
  m._13 = json[2].asFloat();
  m._14 = json[3].asFloat();

  m._21 = json[4].asFloat();
  m._22 = json[5].asFloat();
  m._23 = json[6].asFloat();
  m._24 = json[7].asFloat();

  m._31 = json[8].asFloat();
  m._32 = json[9].asFloat();
  m._33 = json[10].asFloat();
  m._34 = json[11].asFloat();

  m._41 = json[12].asFloat();
  m._42 = json[13].asFloat();
  m._43 = json[14].asFloat();
  m._44 = json[15].asFloat();

  return m;
}

Vector3 DeserializeVectorFromJson(Json::Value& json) {
  Vector3 viktor;

  viktor.x = json[0].asFloat();
  viktor.y = json[1].asFloat();
  viktor.z = json[2].asFloat();

  return viktor;
}

XrStereoPose DecodePoseFromJson(std::string& poseJson) {
  Json::Reader reader;
  Json::Value jsonRoot;

  assert(reader.parse(poseJson, jsonRoot) && "Could not parse pose as valid json :'(");

  auto viewLeft = jsonRoot.get(KEY_POSE_VIEW_LEFT, "invalid");
  assert(viewLeft.isArray() && "Invalid Pose Json");

  auto viewRight = jsonRoot.get(KEY_POSE_VIEW_RIGHT, "invalid");
  assert(viewRight.isArray() && "Invalid Pose Json");

  auto projLeft = jsonRoot.get(KEY_POSE_PROJ_LEFT, "invalid");
  assert(projLeft.isArray() && "Invalid Pose Json");

  auto projRight = jsonRoot.get(KEY_POSE_PROJ_RIGHT, "invalid");
  assert(projRight.isArray() && "Invalid Pose Json");

  auto position = jsonRoot.get(KEY_POSE_CAM_POS, "invalid");
  assert(position.isArray() && "Invalid Pose Json");

  auto orientation = jsonRoot.get(KEY_POSE_CAM_ROT, "invalid");
  assert(orientation.isArray() && "Invalid Pose Json");

  XrStereoPose pose;

  pose.viewLeft = DeserializeMatrixFromJson(viewLeft);
  pose.viewRight = DeserializeMatrixFromJson(viewRight);

  pose.projLeft = DeserializeMatrixFromJson(projLeft);
  pose.projRight = DeserializeMatrixFromJson(projRight);

  pose.cameraPosition = DeserializeVectorFromJson(position);
  pose.cameraOrientation = DeserializeVectorFromJson(orientation);

  return pose;
}

Json::Value SerializeMatrixAsJson(Matrix4x4 m) {
  Json::Value json;
  json[0] = m._11;
  json[1] = m._12;
  json[2] = m._13;
  json[3] = m._14;

  json[4] = m._21;
  json[5] = m._22;
  json[6] = m._23;
  json[7] = m._24;

  json[8] = m._31;
  json[9] = m._32;
  json[10] = m._33;
  json[11] = m._34;

  json[12] = m._41;
  json[13] = m._42;
  json[14] = m._43;
  json[15] = m._44;

  return json;
}

Json::Value SerializeVectorAsJson(Vector3 viktor) {
  Json::Value json;

  json[0] = viktor.x;
  json[1] = viktor.y;
  json[2] = viktor.z;

  return json;
}

std::string SerializePoseAsJson(XrStereoPose& pose) {

  Json::Value json;

  json[KEY_POSE_VIEW_LEFT] = SerializeMatrixAsJson(pose.viewLeft);
  json[KEY_POSE_VIEW_RIGHT] = SerializeMatrixAsJson(pose.viewRight);

  json[KEY_POSE_PROJ_LEFT] = SerializeMatrixAsJson(pose.projLeft);
  json[KEY_POSE_PROJ_RIGHT] = SerializeMatrixAsJson(pose.projRight);

  json[KEY_POSE_CAM_POS] = SerializeVectorAsJson(pose.cameraPosition);
  json[KEY_POSE_CAM_ROT] = SerializeVectorAsJson(pose.cameraOrientation);

  return json.toStyledString();
}