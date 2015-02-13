#include "test/test.h"

#include "src/message.h"
#include "src/errors.h"

#include <iostream>
#include <vector>
#include <stdint.h>

namespace CthunAgent {

TEST_CASE("MessageChunk", "[message]") {
    SECTION("can instantiate with default ctor") {
        REQUIRE_NOTHROW(MessageChunk());
    }

    SECTION("correctly instantiate with default ctor") {
        MessageChunk m_c {};

        REQUIRE(m_c.descriptor == 0);
        REQUIRE(m_c.size == 0);
        REQUIRE(m_c.data_portion == "");
    }

    SECTION("does serialize correctly") {
        MessageChunk chunk { 1, 6, "header" };

        SerializedMessage buffer;
        chunk.serializeOn(buffer);
        REQUIRE(buffer == SerializedMessage({ 1,
                                              0, 0, 0, 6,
                                              'h', 'e', 'a', 'd', 'e', 'r' }));
    }
}

static const MessageChunk envelope_chunk { 1, 16, "I am an envelope" };
static const MessageChunk data_chunk { 2, 14, "some data here" };
static const MessageChunk debug_chunk { 3, 18, "lots of debug info" };

TEST_CASE("Message::Message, getters & inspectors - passing chunks", "[message]") {
    SECTION("can instantiate by passing an envelope chunk") {
        Message msg { envelope_chunk };

        REQUIRE_FALSE(msg.hasData());
        REQUIRE_FALSE(msg.hasDebug());

        auto retrieved_env_chunk    = msg.getEnvelopeChunk();
        auto retrieved_data_chunk   = msg.getDataChunk();
        auto retrieved_debug_chunks = msg.getDebugChunks();

        REQUIRE(retrieved_env_chunk.descriptor == 1);
        REQUIRE(retrieved_data_chunk.descriptor == 0);
        REQUIRE(retrieved_data_chunk.size == 0);
        REQUIRE(retrieved_data_chunk.data_portion == "");
        REQUIRE(retrieved_debug_chunks.empty());
    }

    SECTION("can instantiate by passing a data chunk") {
        Message msg { envelope_chunk, data_chunk };

        REQUIRE(msg.hasData());
        REQUIRE_FALSE(msg.hasDebug());

        auto retrieved_env_chunk    = msg.getEnvelopeChunk();
        auto retrieved_data_chunk   = msg.getDataChunk();
        auto retrieved_debug_chunks = msg.getDebugChunks();

        REQUIRE(retrieved_env_chunk.descriptor == 1);
        REQUIRE(retrieved_data_chunk.descriptor == 2);
        REQUIRE(retrieved_debug_chunks.empty());
    }

    SECTION("can instantiate by passing a debug chunk") {
        Message msg { envelope_chunk, data_chunk, debug_chunk };

        REQUIRE(msg.hasData());
        REQUIRE(msg.hasDebug());

        auto retrieved_env_chunk    = msg.getEnvelopeChunk();
        auto retrieved_data_chunk   = msg.getDataChunk();
        auto retrieved_debug_chunks = msg.getDebugChunks();

        REQUIRE(retrieved_env_chunk.descriptor == 1);
        REQUIRE(retrieved_data_chunk.descriptor == 2);
        REQUIRE(retrieved_debug_chunks.size() == 1);
        REQUIRE(retrieved_debug_chunks[0].descriptor == 3);
    }

    SECTION("throws if a passed chunk has an invalid size") {
        MessageChunk bad_env_c { 1, 128, "I am a badly sized envelope" };
        MessageChunk bad_data_c { 2, 2, "I am a badly sized data chunk" };
        MessageChunk bad_debug_c { 3, 10983, "I am a badly sized debug chunk" };

        REQUIRE_THROWS_AS(Message { bad_env_c },
                          message_processing_error);
        REQUIRE_THROWS_AS(Message(envelope_chunk, bad_data_c),
                          message_processing_error);
        REQUIRE_THROWS_AS(Message(envelope_chunk, data_chunk, bad_debug_c),
                          message_processing_error);
    }

    SECTION("throws if a passed chunk has an unknown descriptor") {
        MessageChunk bad_env_c { 7, 10, "tenletters" };
        MessageChunk bad_data_c { 8, 10, "tenletters" };
        MessageChunk bad_debug_c { 9, 10, "tenletters" };

        REQUIRE_THROWS_AS(Message { bad_env_c },
                          message_processing_error);
        REQUIRE_THROWS_AS(Message(envelope_chunk, bad_data_c),
                          message_processing_error);
        REQUIRE_THROWS_AS(Message(envelope_chunk, data_chunk, bad_debug_c),
                          message_processing_error);
    }
}

static const SerializedMessage msg_buffer_valid {
    0x01,                           // version
    0x01,                           // envelope descriptor: 0000 0001
    0, 0, 0, 0x06,                  // envelope size
    'h', 'e', 'a', 'd', 'e', 'r'    // envelope data portion
};

static const SerializedMessage data_chunk_buffer_valid {
    0x02,                           // data descriptor: 0000 0010
    0, 0, 0, 0x05,                  // data size
    's', 't', 'u', 'f', 'f'         // data data portion (!)
};

static const SerializedMessage debug_chunk_buffer_valid_1 {
    0x03,                           // debug descriptor: 0000 0011
    0, 0, 0, 0x06,                  // debug size
    'e', 'r', 'r', 'o', 'r', 's'    // debug data portion
};

static const SerializedMessage debug_chunk_buffer_valid_2 {
    0x13,                           // debug descriptor: 0001 0011
    0, 0, 0, 0x5 ,                  // debug size
    's', 't', 'a', 't', 's'         // debug data portion
};

using SM_V = std::vector<SerializedMessage>;

std::string vecToStr(SerializedMessage msg_buffer,
                     const SM_V& optional_chunks = SM_V()) {
    for (const auto& c : optional_chunks) {
        msg_buffer.reserve(c.size());
        msg_buffer.insert(msg_buffer.end(), c.begin(), c.end());
    }

    return std::string(msg_buffer.begin(), msg_buffer.end());
}

TEST_CASE("Message::Message - parsing valid messages", "[message]") {
    SECTION("can instantiate by passing a valid message") {
        REQUIRE_NOTHROW(Message(vecToStr(msg_buffer_valid)));
    }

    SECTION("parse correctly a message that has only the envelope") {
        Message msg { vecToStr(msg_buffer_valid) };

        REQUIRE(msg.getVersion() == 1);
        REQUIRE_FALSE(msg.hasData());
        REQUIRE_FALSE(msg.hasDebug());

        auto e_c = msg.getEnvelopeChunk();
        REQUIRE(e_c.descriptor == 1);
        REQUIRE(e_c.size == 6);
        REQUIRE(e_c.data_portion == "header");
    }

    SECTION("parse correctly a message that has data") {
        Message msg { vecToStr(msg_buffer_valid, { data_chunk_buffer_valid }) };

        REQUIRE(msg.getVersion() == 1);
        REQUIRE(msg.hasData());
        REQUIRE_FALSE(msg.hasDebug());

        auto e_c = msg.getEnvelopeChunk();
        REQUIRE(e_c.descriptor == 1);
        REQUIRE(e_c.size == 6);
        REQUIRE(e_c.data_portion == "header");

        auto d_c = msg.getDataChunk();
        REQUIRE(d_c.descriptor == 2);
        REQUIRE(d_c.size == 5);
        REQUIRE(d_c.data_portion == "stuff");
    }

    SECTION("parse correctly a message that has a single debug chunk") {
        auto msg_s = vecToStr(msg_buffer_valid, { debug_chunk_buffer_valid_1 });
        Message msg { msg_s };

        REQUIRE(msg.getVersion() == 1);
        REQUIRE_FALSE(msg.hasData());
        REQUIRE(msg.hasDebug());

        auto e_c = msg.getEnvelopeChunk();
        REQUIRE(e_c.descriptor == 1);
        REQUIRE(e_c.size == 6);
        REQUIRE(e_c.data_portion == "header");

        auto db_chunks = msg.getDebugChunks();
        REQUIRE(db_chunks.size() == 1);
        REQUIRE(db_chunks[0].descriptor == 3);
        REQUIRE(db_chunks[0].size == 6);
        REQUIRE(db_chunks[0].data_portion == "errors");
    }

    SECTION("parse correctly a message that has multiple debug chunks") {
        auto msg_s = vecToStr(msg_buffer_valid, { debug_chunk_buffer_valid_1,
                                                  debug_chunk_buffer_valid_2,
                                                  debug_chunk_buffer_valid_1,
                                                  debug_chunk_buffer_valid_1,
                                                  debug_chunk_buffer_valid_1 });
        Message msg { msg_s };

        REQUIRE(msg.getVersion() == 1);
        REQUIRE_FALSE(msg.hasData());
        REQUIRE(msg.hasDebug());

        auto e_c = msg.getEnvelopeChunk();
        REQUIRE(e_c.descriptor == 1);
        REQUIRE(e_c.size == 6);
        REQUIRE(e_c.data_portion == "header");

        auto db_chunks = msg.getDebugChunks();
        REQUIRE(db_chunks.size() == 5);

        REQUIRE(db_chunks[0].descriptor == 0x03);
        REQUIRE(db_chunks[0].size == 6);
        REQUIRE(db_chunks[0].data_portion == "errors");

        REQUIRE(db_chunks[1].descriptor == 0x13);
        REQUIRE(db_chunks[1].size == 5);
        REQUIRE(db_chunks[1].data_portion == "stats");

        REQUIRE(db_chunks[4].descriptor == 0x03);
        REQUIRE(db_chunks[4].size == 6);
        REQUIRE(db_chunks[4].data_portion == "errors");
    }

    SECTION("parse correctly a message that has data and debug chunks") {
        auto msg_s = vecToStr(msg_buffer_valid, { data_chunk_buffer_valid,
                                                  debug_chunk_buffer_valid_1,
                                                  debug_chunk_buffer_valid_2,
                                                  debug_chunk_buffer_valid_2,
                                                  debug_chunk_buffer_valid_2 });
        Message msg { msg_s };

        REQUIRE(msg.getVersion() == 1);
        REQUIRE(msg.hasData());
        REQUIRE(msg.hasDebug());

        auto e_c = msg.getEnvelopeChunk();
        REQUIRE(e_c.descriptor == 1);
        REQUIRE(e_c.size == 6);
        REQUIRE(e_c.data_portion == "header");

        auto d_c = msg.getDataChunk();
        REQUIRE(d_c.descriptor == 2);
        REQUIRE(d_c.size == 5);
        REQUIRE(d_c.data_portion == "stuff");

        auto db_chunks = msg.getDebugChunks();
        REQUIRE(db_chunks.size() == 4);

        REQUIRE(db_chunks[3].descriptor == 0x13);
        REQUIRE(db_chunks[3].size == 5);
        REQUIRE(db_chunks[3].data_portion == "stats");
    }

}

TEST_CASE("Message::Message - failure cases", "[message]") {
    SECTION("fails if the message doesn't have the envelope") {
        SerializedMessage msg_buffer_bad { 0x01 };  // has only version
        auto msg_s = vecToStr(msg_buffer_bad, { data_chunk_buffer_valid });

        REQUIRE_THROWS_AS(Message { msg_s }, message_processing_error);
    }

    SECTION("fails if the message version is not supported") {
        SerializedMessage msg_buffer_bad = msg_buffer_valid;
        msg_buffer_bad[0] = { 0xFF };  // setting a bad version
        auto msg_s = vecToStr(msg_buffer_bad, { data_chunk_buffer_valid });

        REQUIRE_THROWS_AS(Message { msg_s }, message_processing_error);
    }

    SECTION("fails if the message contains a chunk with an invalid descriptor") {
        SerializedMessage data_chunk_bad = data_chunk_buffer_valid;
        data_chunk_bad[0] = { 0xFA };  // setting a bad descritptor
        auto msg_s = vecToStr(msg_buffer_valid, { data_chunk_bad });

        REQUIRE_THROWS_AS(Message { msg_s }, message_processing_error);
    }
}

TEST_CASE("Message modifiers", "[message]") {
    // add multiple debug

    // descriptors with 4 most significant bits changed
}

}  // namespace CthunAgent
