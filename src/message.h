#ifndef CTHUN_SRC_MESSAGE_H_
#define CTHUN_SRC_MESSAGE_H_

#include "src/data_container.h"
#include "src/message_serialization.h"

#include <string>
#include <vector>

namespace CthunAgent {

// TODO(ale): add descriptors when needed (compression, type...)
namespace ChunkDescriptor {
    static const uint8_t ENVELOPE { 1 };
    static const uint8_t PAYLOAD_JSON { 41 };
    static const uint8_t DEBUG { 81 };
}  // namespace ChunkDescriptor

struct MessageChunk {
    uint8_t descriptor;
    DataContainer data;

    uint32_t size();  // [byte]
};

class Message {
  public:
    // The default ctor is deleted since, for the Cthun protocol, a
    // valid message must have an envelope chunk (invariant)
    Message() = delete;

    // Construct a Message by deserializing the payload delivered
    // by the transport layer as a std::string
    explicit Message(const std::string& transport_payload);

    // Create a new message with a given envelope
    explicit Message(MessageChunk envelope);

    // ... and a data chunk
    Message(MessageChunk envelope,
            MessageChunk payload);

    // ... and a debug chunk
    Message(MessageChunk envelope,
            MessageChunk payload,
            MessageChunk debug);

    // Add optional chunks
    void addPayload(MessageChunk payload);
    void addDebug(MessageChunk debug);

    // Get chunks
    MessageChunk getEnvelope();
    std::vector<MessageChunk> getPayload();
    std::vector<MessageChunk> getAllDebug();

    // Inspectors
    bool hasPayload();
    bool hasDebug();

    // Get the serialized message for transport over websocketpp
    SerializedMessage getSerialized();

  private:
    MessageChunk envelope_;
    std::vector<MessageChunk> payload_;
    std::vector<MessageChunk> debug_;
};

}  // namespace CthunAgent

#endif  // CTHUN_SRC_DATA_CONTAINER_H_
