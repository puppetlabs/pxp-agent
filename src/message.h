#ifndef CTHUN_SRC_MESSAGE_H_
#define CTHUN_SRC_MESSAGE_H_

#include "src/data_container.h"
#include "src/message_serialization.h"

#include <string>
namespace CthunAgent {


// Serialization

// std::string

// serialize a 32 bit integer into network format (big-endian)


// TODO(ale): add more descriptors
namespace ChunkDescriptor {
    static const uint8_t ENVELOPE { 1 };
    static const uint8_t DATA_JSON { 41 };
    static const uint8_t DEBUG { 81 };
}  // namespace ChunkDescriptor

struct MessageChunk {
    uint8_t descriptor;
    uint32_t size;
    std::string data;
};

// TODO: deriving from DataContainer to shut up the old code

class Message : DataContainer {
  public:
    // The default ctor is deleted since, for the Cthun protocol, a
    // valid message must have an envelope chunk; that's an invariant
    Message() = delete;

    // Construct a Message by deserializing a raw message
    explicit Message(const std::string& binary_payload);

    // Create a new message with a given envelope
    explicit Message(DataContainer envelope);

    // ... and a data chunk
    Message(DataContainer envelope,
            DataContainer data);

    // ... and a debug chunk
    Message(DataContainer envelope,
            DataContainer data,
            DataContainer debug);

    // Set fields
    void setHeader(DataContainer header);
    void setData(DataContainer data);
    void setDebug(DataContainer debug);

    // Get fields
    DataContainer getHeader();
    DataContainer getData();

    // TODO: assuming that a message can contain multiple debug chunks
    std::vector<DataContainer> getAllDebug();

    // Inspectors
    bool hasData();
    bool hasDebug();

    // serialize message for transport over websocketpp
    MessageBuffer serialize();





  // private:
  //   int getChunkSize_(DataContainer& chunk);




  // public:

  //   explicit Message(std::string msg) : DataContainer(msg) {}
  //   explicit Message(const rapidjson::Value& value) : DataContainer(value) {}
  //   Message(const Message& msg) : DataContainer(msg) {}
  //   Message(const Message&& msg) : DataContainer(msg) {}
  //   Message& operator=(Message other) {
  //       DataContainer::operator=(other);
  //       return *this;
  //   }
};

}  // namespace CthunAgent

#endif  // CTHUN_SRC_DATA_CONTAINER_H_
