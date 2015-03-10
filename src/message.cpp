#include "src/message.h"
#include "src/errors.h"
#include "src/string_utils.h"
#include "src/data_parser.h"

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.cthun_agent.message"
#include <leatherman/logging/logging.hpp>

#include <algorithm>  // find

// TODO(ale): disable assert() once we're confident with the code...
// To disable assert()
// #define NDEBUG
#include <cassert>

namespace CthunAgent {

//
// Constants
//

// TODO(ale): determine value based on the expected json entries
// Min size of the envelope chunk data [byte]
static const size_t MIN_ENVELOPE_SIZE { 6 };

// Size of descriptor and size fields [byte]
static const size_t CHUNK_METADATA_SIZE { 5 };

// Size of version field [byte]
static const size_t VERSION_FIELD_SIZE { 1 };

// Ordered list of supported protocol versions; the last entry should
// be used when creating new messages
static std::vector<uint8_t> SUPPORTED_VERSIONS { 1 };

//
// MessageChunk
//

MessageChunk::MessageChunk() : descriptor { 0 },
                               size { 0 },
                               content {} {
}

MessageChunk::MessageChunk(uint8_t _descriptor,
                           uint32_t _size,
                           std::string _content)
        : descriptor { _descriptor },
          size { _size },
          content { _content } {
}

MessageChunk::MessageChunk(uint8_t _descriptor,
                           std::string _content)
        : MessageChunk(_descriptor, _content.size(), _content) {
}

bool MessageChunk::operator==(const MessageChunk& other_msg_chunk) const {
    return descriptor == other_msg_chunk.descriptor
           && size == other_msg_chunk.size
           && content == other_msg_chunk.content;
}

void MessageChunk::serializeOn(SerializedMessage& buffer) const {
    serialize<uint8_t>(descriptor, 1, buffer);
    serialize<uint32_t>(size, 4, buffer);
    serialize<std::string>(content, size, buffer);
}

std::string MessageChunk::toString() const {
     return std::to_string(descriptor) + std::to_string(size) + content;
}

//
// Message
//

// Constructors

Message::Message(const std::string& transport_msg) : version_ {},
                                                     envelope_chunk_ {},
                                                     data_chunk_ {},
                                                     debug_chunks_ {} {
    parseMessage_(transport_msg);
}

Message::Message(MessageChunk envelope_chunk)
        : version_ { SUPPORTED_VERSIONS.back() },
          envelope_chunk_ { envelope_chunk },
          data_chunk_ {},
          debug_chunks_ {} {
    validateChunk_(envelope_chunk);
}

Message::Message(MessageChunk envelope_chunk, MessageChunk data_chunk)
        : version_ { SUPPORTED_VERSIONS.back() },
          envelope_chunk_ { envelope_chunk },
          data_chunk_ { data_chunk },
          debug_chunks_ {} {
    validateChunk_(envelope_chunk);
    validateChunk_(data_chunk);
}

Message::Message(MessageChunk envelope_chunk, MessageChunk data_chunk,
                 MessageChunk debug_chunk)
        : version_ { SUPPORTED_VERSIONS.back() },
          envelope_chunk_ { envelope_chunk },
          data_chunk_ { data_chunk },
          debug_chunks_ { debug_chunk } {
    validateChunk_(envelope_chunk);
    validateChunk_(data_chunk);
    validateChunk_(debug_chunk);
}

// Add chunks

void Message::setDataChunk(MessageChunk data_chunk) {
    validateChunk_(data_chunk);

    if (hasData()) {
        LOG_WARNING("Resetting data chunk");
    }

    data_chunk_ = data_chunk;
}

void Message::addDebugChunk(MessageChunk debug_chunk) {
    validateChunk_(debug_chunk);
    debug_chunks_.push_back(debug_chunk);
}

// Getters

uint8_t Message::getVersion() const {
    return version_;
}

MessageChunk Message::getEnvelopeChunk() const {
    return envelope_chunk_;
}

MessageChunk Message::getDataChunk() const {
    return data_chunk_;
}

std::vector<MessageChunk> Message::getDebugChunks() const {
    return debug_chunks_;
}

// Inspectors

bool Message::hasData() const {
    return data_chunk_.descriptor != 0;
}

bool Message::hasDebug() const {
    return !debug_chunks_.empty();
}

// Get the serialized message

SerializedMessage Message::getSerialized() const {
    SerializedMessage buffer;

    // Version (mandatory)
    serialize<uint8_t>(version_, 1, buffer);

    // Envelope (mandatory)
    envelope_chunk_.serializeOn(buffer);

    if (hasData()) {
        // Data (optional)
        data_chunk_.serializeOn(buffer);
    }

    if (hasDebug()) {
        for (const auto& d_c : debug_chunks_)
            // Debug (optional; mutiple)
            d_c.serializeOn(buffer);
    }

    return buffer;
}

// Parse, validate, and return the content of chunks

ParsedContent Message::getParsedContent() const {
    auto envelope_content = DataParser::parseAndValidateChunk(envelope_chunk_);

    DataContainer data_content {};
    if (hasData()) {
        auto schema = envelope_content.get<std::string>("data_schema");
        assert(SchemaFormat.find(schema) != SchemaFormat.end());
        data_content = DataParser::parseAndValidateChunk(data_chunk_,
                                                         SchemaFormat[schema]);
    }

    std::vector<DataContainer> debug_content {};
    for (const auto& d_c : debug_chunks_) {
        debug_content.push_back(DataParser::parseAndValidateChunk(d_c));
    }

    return ParsedContent { envelope_content, data_content, debug_content };
}

// toString

std::string Message::toString() const {
    auto s = std::to_string(version_) + envelope_chunk_.toString();

    if (hasData()) {
        s += data_chunk_.toString();
    }

    for (const auto& debug_chunk : debug_chunks_) {
        s += debug_chunk.toString();
    }

    return s;
}

//
// Message - private interface
//

void Message::parseMessage_(const std::string& transport_msg) {
    auto msg_size = transport_msg.size();

    if (msg_size < MIN_ENVELOPE_SIZE) {
        LOG_ERROR("Invalid msg; envelope is too small");
        LOG_TRACE("Invalid msg content (unserialized): '%1%'", transport_msg);
        throw message_processing_error { "invalid msg: envelope too small" };
    }

    // Serialization buffer

    auto buffer = SerializedMessage(transport_msg.begin(), transport_msg.end());
    SerializedMessage::const_iterator next_itr { buffer.begin() };

    // Version

    auto msg_version = deserialize<uint8_t>(1, next_itr);
    validateVersion_(msg_version);

    // Envelope (mandatory chunk)

    auto envelope_desc = deserialize<uint8_t>(1, next_itr);
    auto envelope_desc_bit = envelope_desc & ChunkDescriptor::TYPE_MASK;
    if (envelope_desc_bit != ChunkDescriptor::ENVELOPE) {
        LOG_ERROR("Invalid msg; missing envelope descriptor");
        LOG_TRACE("Invalid msg content (unserialized): '%1%'", transport_msg);
        throw message_processing_error { "invalid msg: no envelope descriptor" };
    }

    auto envelope_size = deserialize<uint32_t>(4, next_itr);
    if (msg_size < VERSION_FIELD_SIZE + CHUNK_METADATA_SIZE + envelope_size) {
        LOG_ERROR("Invalid msg; missing envelope content");
        LOG_TRACE("Invalid msg content (unserialized): '%1%'", transport_msg);
        throw message_processing_error { "invalid msg: no envelope" };
    }

    auto envelope_content = deserialize<std::string>(envelope_size, next_itr);

    // Data and debug (optional chunks)

    auto still_to_parse =  msg_size - (VERSION_FIELD_SIZE + CHUNK_METADATA_SIZE
                                       + envelope_size);

    while (still_to_parse > CHUNK_METADATA_SIZE) {
        auto chunk_desc = deserialize<uint8_t>(1, next_itr);
        auto chunk_desc_bit = chunk_desc & ChunkDescriptor::TYPE_MASK;

        if (chunk_desc_bit != ChunkDescriptor::DATA
                && chunk_desc_bit != ChunkDescriptor::DEBUG) {
            LOG_ERROR("Invalid msg; invalid chunk descriptor %1%",
                      static_cast<int>(chunk_desc));
            LOG_TRACE("Invalid msg content (unserialized): '%1%'", transport_msg);
            throw message_processing_error { "invalid msg: invalid "
                                             "chunk descriptor" };
        }

        auto chunk_size = deserialize<uint32_t>(4, next_itr);
        if (chunk_size > still_to_parse - CHUNK_METADATA_SIZE) {
            LOG_ERROR("Invalid msg; missing part of the %1% chunk content (%2% "
                      "byte%3% declared - missing %4% byte%5%)",
                      ChunkDescriptor::names[chunk_desc_bit],
                      chunk_size, StringUtils::plural(chunk_size),
                      still_to_parse - CHUNK_METADATA_SIZE,
                      StringUtils::plural(still_to_parse - CHUNK_METADATA_SIZE));
            LOG_TRACE("Invalid msg content (unserialized): '%1%'", transport_msg);
            throw message_processing_error { "invalid msg: missing chunk content" };
        }

        auto chunk_content = deserialize<std::string>(chunk_size, next_itr);
        MessageChunk chunk { chunk_desc, chunk_size, chunk_content };

        if (chunk_desc_bit == ChunkDescriptor::DATA) {
            if (hasData()) {
                LOG_ERROR("Invalid msg; multiple data chunks");
                LOG_TRACE("Invalid msg content (unserialized): '%1%'",
                          transport_msg);
                throw message_processing_error { "invalid msg: multiple data "
                                                 "chunks" };
            }

            data_chunk_ = chunk;
        } else {
            debug_chunks_.push_back(chunk);
        }

        still_to_parse -= CHUNK_METADATA_SIZE + chunk_size;
    }

    if (still_to_parse > 0) {
        LOG_ERROR("Failed to parse the entire msg (ignoring last %1% byte%2%); "
                  "the msg will be processed anyway",
                  still_to_parse, StringUtils::plural(still_to_parse));
        LOG_TRACE("Msg content (unserialized): '%1%'", transport_msg);
    }

    version_ = msg_version;
    envelope_chunk_ = MessageChunk  { envelope_desc,
                                      envelope_size,
                                      envelope_content };
}

void Message::validateVersion_(const uint8_t& version) const {
    auto found = std::find(SUPPORTED_VERSIONS.begin(), SUPPORTED_VERSIONS.end(),
                           version);
    if (found == SUPPORTED_VERSIONS.end()) {
        LOG_ERROR("Unsupported message version: %1%", static_cast<int>(version));
        throw message_processing_error { "unsupported message version" };
    }
}

void Message::validateChunk_(const MessageChunk& chunk) const {
    auto desc_bit = chunk.descriptor & ChunkDescriptor::TYPE_MASK;

    if (ChunkDescriptor::names.find(desc_bit) == ChunkDescriptor::names.end()) {
        LOG_ERROR("Unknown chunk descriptor: %1%",
                  static_cast<int>(chunk.descriptor));
        throw message_processing_error { "unknown descriptor" };
    }

    if (chunk.size != static_cast<uint32_t>(chunk.content.size())) {
        LOG_ERROR("Incorrect size for %1% chunk; declared %2% byte%3%, "
                  "got %4% byte%5%", ChunkDescriptor::names[desc_bit],
                  chunk.size, StringUtils::plural(chunk.size),
                  chunk.content.size(),
                  StringUtils::plural(chunk.content.size()));
        throw message_processing_error { "invalid size" };
    }
}

}  // namespace CthunAgent
