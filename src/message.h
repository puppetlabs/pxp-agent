#ifndef CTHUN_SRC_MESSAGE_H_
#define CTHUN_SRC_MESSAGE_H_

#include "src/data_container.h"

namespace CthunAgent {

class Message : public DataContainer {
  public:
    Message() {}
    explicit Message(std::string msg) : DataContainer(msg) {}
    explicit Message(const rapidjson::Value& value) : DataContainer(value) {}
    Message(const Message& msg) : DataContainer(msg) {}
    Message(const Message&& msg) : DataContainer(msg) {}
    Message& operator=(Message other) {
        DataContainer::operator=(other);
        return *this;
    }
};

}  // namespace CthunAgent

#endif  // CTHUN_SRC_DATA_CONTAINER_H_
