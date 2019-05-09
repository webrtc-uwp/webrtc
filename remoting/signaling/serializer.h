#ifndef HOLOLIGHT_SIGNALING_SERIALIZER_H
#define HOLOLIGHT_SIGNALING_SERIALIZER_H

#include "signaling_relay.h"

#include <string>

namespace hololight::signaling {

class Serializer {
 public:
  virtual ~Serializer(){};

  virtual std::string serialize(RelayMessage& message) = 0;
  virtual RelayMessage deserialize(std::string message) = 0;
};

}  // namespace hololight::signaling

#endif  // HOLOLIGHT_SIGNALING_SERIALIZER_H