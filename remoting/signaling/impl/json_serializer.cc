#include "json_serializer.h"
// this include is weird because webrtc's internal json uses
// third_party/jsoncpp/json.h but the compiler complains it can't find the
// header if we do that
#include "third_party/jsoncpp/source/include/json/json.h"
// #include "third_party/jsoncpp/json.h"

using namespace hololight::signaling;
using namespace hololight::signaling::impl;

std::string JsonSerializer::serialize(RelayMessage& message) {
  Json::Value root;
  root["$type"] = "RelayedTextMessage";
  root["ContentType"] = message.contentType;
  root["Content"] = message.content;

  // nlohmann::json j = {
  // 	{ "$type", "RelayedTextMessage" },
  // 	{ "ContentType", message.contentType },
  // 	{ "Content", message.content }
  // };

  // return j.dump();

  return root.toStyledString();
}

RelayMessage JsonSerializer::deserialize(std::string message) {
  Json::Reader reader;
  Json::Value root;
  reader.parse(message, root);
  // auto json = nlohmann::json::parse(message);
  
  auto contentType = root.get("ContentType", "invalid").asString();
  auto content = root.get("Content", "blergh").asString();

  return RelayMessage(contentType, content);
}
