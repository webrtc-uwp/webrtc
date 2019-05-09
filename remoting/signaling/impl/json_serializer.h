#ifndef HOLOLIGHT_SIGNALING_IMPL_JSONSERIALIZER_H
#define HOLOLIGHT_SIGNALING_IMPL_JSONSERIALIZER_H

#include "../serializer.h"

namespace hololight::signaling::impl {

class JsonSerializer : public Serializer {
 public:
  std::string serialize(RelayMessage& message);
  hololight::signaling::RelayMessage deserialize(std::string message);
};
}  // namespace hololight::signaling::impl

#endif  // HOLOLIGHT_SIGNALING_IMPL_UWPTCPRELAY_H