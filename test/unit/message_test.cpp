#include "test/test.h"

#include "src/message.h"
#include "src/errors.h"

#include <iostream>
#include <vector>
#include <stdint.h>
#include <chrono>

namespace CthunAgent {

TEST_CASE("MessageChunk", "[message]") {
    SECTION("can instantiate with default ctor") {
        REQUIRE_NOTHROW(MessageChunk());
    }

    SECTION("correctly instantiate with default ctor") {
        MessageChunk m_c {};

        REQUIRE(m_c.descriptor == 0);
        REQUIRE(m_c.size == 0);
        REQUIRE(m_c.content == "");
    }

    SECTION("correctly instantiate without passing the data size") {
        MessageChunk m_c { 0x01, "lalala" };

        REQUIRE(m_c.descriptor == 0x01);
        REQUIRE(m_c.size == 6);
        REQUIRE(m_c.content == "lalala");

        MessageChunk m_c_multi_byte_char { 0x02, "a β requires two bytes" };

        REQUIRE(m_c_multi_byte_char.descriptor == 0x02);
        REQUIRE(m_c_multi_byte_char.size == 23);
        REQUIRE(m_c_multi_byte_char.content == "a β requires two bytes");
    }

    SECTION("serialize correctly") {
        MessageChunk chunk { 0x01, 0x06, "header" };

        SerializedMessage buffer;
        chunk.serializeOn(buffer);
        REQUIRE(buffer == SerializedMessage({ 0x01,
                                              0, 0, 0, 6,
                                              'h', 'e', 'a', 'd', 'e', 'r' }));
    }
}

static const MessageChunk envelope_chunk { 0x01, 16, "I am an envelope" };
static const MessageChunk data_chunk { 0x02, 14, "some data here" };
static const MessageChunk debug_chunk { 0x03, 18, "lots of debug info" };

TEST_CASE("MessageChunk::toString", "[message]") {
    SECTION("can get the string representation of the chunk") {
        REQUIRE_NOTHROW(envelope_chunk.toString());
    }

    SECTION("the string representation of the chunk is correct") {
        auto s = envelope_chunk.toString();  // "1" + "16" + "I am an envelope"
        REQUIRE(s.size() == 1 + 2 + envelope_chunk.size);
        REQUIRE(s.substr(1 + 2, std::string::npos) == envelope_chunk.content);
    }
}

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
        REQUIRE(retrieved_data_chunk.content == "");
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
    'h', 'e', 'a', 'd', 'e', 'r'    // envelope content
};

static const SerializedMessage data_chunk_buffer_valid {
    0x02,                           // data descriptor: 0000 0010
    0, 0, 0, 0x05,                  // data size
    's', 't', 'u', 'f', 'f'         // data content
};

static const SerializedMessage debug_chunk_buffer_valid_1 {
    0x03,                           // debug descriptor: 0000 0011
    0, 0, 0, 0x06,                  // debug size
    'e', 'r', 'r', 'o', 'r', 's'    // debug content
};

static const SerializedMessage debug_chunk_buffer_valid_2 {
    0x13,                           // debug descriptor: 0001 0011
    0, 0, 0, 0x05 ,                 // debug size
    's', 't', 'a', 't', 's'         // debug content
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

void checkChunk(const MessageChunk& m_c,
                const uint8_t descriptor,
                const uint32_t size,
                const std::string& content) {
    REQUIRE(m_c.descriptor == descriptor);
    REQUIRE(m_c.size == size);
    REQUIRE(m_c.content == content);
}

TEST_CASE("Message::Message - parsing valid messages", "[message]") {
    SECTION("can instantiate by passing a valid message") {
        REQUIRE_NOTHROW(Message(vecToStr(msg_buffer_valid)));
    }

    SECTION("parse correctly a message that has only the envelope") {
        Message msg { vecToStr(msg_buffer_valid) };
        // Overall message
        REQUIRE(msg.getVersion() == 0x01);
        REQUIRE_FALSE(msg.hasData());
        REQUIRE_FALSE(msg.hasDebug());
        // Envelope
        checkChunk(msg.getEnvelopeChunk(), 0x01, 0x06, "header");
    }

    SECTION("parse correctly a message that has data") {
        Message msg { vecToStr(msg_buffer_valid, { data_chunk_buffer_valid }) };
        // Overall message
        REQUIRE(msg.getVersion() == 0x01);
        REQUIRE(msg.hasData());
        REQUIRE_FALSE(msg.hasDebug());
        // Envelope
        checkChunk(msg.getEnvelopeChunk(), 0x01, 0x06, "header");
        // Debug
        checkChunk(msg.getDataChunk(), 0x02, 0x05, "stuff");
    }

    SECTION("parse correctly a message that has a single debug chunk") {
        auto msg_s = vecToStr(msg_buffer_valid, { debug_chunk_buffer_valid_1 });
        Message msg { msg_s };
        // Overall message
        REQUIRE(msg.getVersion() == 0x01);
        REQUIRE_FALSE(msg.hasData());
        REQUIRE(msg.hasDebug());
        // Envelope
        checkChunk(msg.getEnvelopeChunk(), 0x01, 0x06, "header");
        // Debug
        auto db_chunks = msg.getDebugChunks();
        REQUIRE(db_chunks.size() == 0x01);
        checkChunk(db_chunks[0], 0x03, 0x06, "errors");
    }

    SECTION("parse correctly a message that has multiple debug chunks") {
        auto msg_s = vecToStr(msg_buffer_valid, { debug_chunk_buffer_valid_1,
                                                  debug_chunk_buffer_valid_2,
                                                  debug_chunk_buffer_valid_1,
                                                  debug_chunk_buffer_valid_1,
                                                  debug_chunk_buffer_valid_1 });
        Message msg { msg_s };
        // Overall message
        REQUIRE(msg.getVersion() == 0x01);
        REQUIRE_FALSE(msg.hasData());
        REQUIRE(msg.hasDebug());
        // Envelope
        checkChunk(msg.getEnvelopeChunk(), 0x01, 0x06, "header");
        // Debug
        auto db_chunks = msg.getDebugChunks();
        REQUIRE(db_chunks.size() == 0x05);
        checkChunk(db_chunks[1], 0x13, 0x05, "stats");
        for (auto i : std::vector<int>({ 0, 2, 3, 4 }))
            checkChunk(db_chunks[i], 0x03, 0x06, "errors");
    }

    SECTION("parse correctly a message that has data and debug chunks") {
        auto msg_s = vecToStr(msg_buffer_valid, { data_chunk_buffer_valid,
                                                  debug_chunk_buffer_valid_1,
                                                  debug_chunk_buffer_valid_2,
                                                  debug_chunk_buffer_valid_2,
                                                  debug_chunk_buffer_valid_2 });
        Message msg { msg_s };
        // Overall message
        REQUIRE(msg.getVersion() == 0x01);
        REQUIRE(msg.hasData());
        REQUIRE(msg.hasDebug());
        // Envelope
        checkChunk(msg.getEnvelopeChunk(), 0x01, 0x06, "header");
        // Data
        checkChunk(msg.getDataChunk(), 0x02, 0x05, "stuff");
        // Debug
        auto db_chunks = msg.getDebugChunks();
        REQUIRE(db_chunks.size() == 0x04);
        checkChunk(db_chunks[0], 0x03, 0x06, "errors");
        for (auto i : std::vector<int>({ 1, 2, 3 }))
            checkChunk(db_chunks[i], 0x13, 0x05, "stats");
    }
}

TEST_CASE("Message::Message - failure cases", "[message]") {
    SECTION("fails to parse if the message doesn't have an envelope") {
        SerializedMessage msg_buffer_bad { 0x01 };  // has only version
        auto msg_s = vecToStr(msg_buffer_bad, { data_chunk_buffer_valid });

        REQUIRE_THROWS_AS(Message { msg_s }, message_processing_error);
    }

    SECTION("fails to parse if the message version is not supported") {
        SerializedMessage msg_buffer_bad = msg_buffer_valid;
        msg_buffer_bad[0] = { 0xFF };  // setting a bad version
        auto msg_s = vecToStr(msg_buffer_bad, { data_chunk_buffer_valid });

        REQUIRE_THROWS_AS(Message { msg_s }, message_processing_error);
    }

    SECTION("fails to parse if the message contains a chunk with an invalid "
             "descriptor") {
        SerializedMessage data_chunk_bad = data_chunk_buffer_valid;
        data_chunk_bad[0] = { 0xFA };  // setting a bad descritptor
        auto msg_s = vecToStr(msg_buffer_valid, { data_chunk_bad });

        REQUIRE_THROWS_AS(Message { msg_s }, message_processing_error);
    }

    SECTION("fails to parse if the message contains two data chunks") {
        auto msg_s = vecToStr(msg_buffer_valid, { data_chunk_buffer_valid,
                                                  data_chunk_buffer_valid });

        REQUIRE_THROWS_AS(Message { msg_s }, message_processing_error);
    }

    SECTION("fails to parse if the envelope content is smaller than what "
            "state in the chunk size") {
        SerializedMessage msg_buffer_bad = msg_buffer_valid;
        msg_buffer_bad[5] = { 0x10 };  // increasing the size
        auto msg_s = vecToStr(msg_buffer_bad);

        REQUIRE_THROWS_AS(Message { msg_s }, message_processing_error);
    }
}

TEST_CASE("Message modifiers", "[message]") {
    Message msg { vecToStr(msg_buffer_valid) };

    SECTION("set the data chunk") {
        msg.setDataChunk(data_chunk);
        REQUIRE(msg.hasData());
    }

    SECTION("add a debug chunk") {
        msg.addDebugChunk(debug_chunk);
        REQUIRE(msg.hasDebug());
    }

    SECTION("set data and add debug chunks") {
        msg.setDataChunk(data_chunk);
        msg.addDebugChunk(debug_chunk);
        REQUIRE(msg.hasData());
        REQUIRE(msg.hasDebug());
    }

    SECTION("add a multiple debug chunks") {
        msg.addDebugChunk(debug_chunk);
        msg.addDebugChunk(debug_chunk);
        REQUIRE(msg.getDebugChunks().size() == 2);
    }

    SECTION("add chunks with descriptors with 4 most significant bits "
            "different than ") {
        auto data_chunk_mod = data_chunk;
        data_chunk_mod.descriptor = 0x92;

        auto debug_chunk_mod = debug_chunk;
        debug_chunk_mod.descriptor = 0x73;

        msg.setDataChunk(data_chunk_mod);
        REQUIRE(msg.hasData());

        msg.addDebugChunk(debug_chunk_mod);
        REQUIRE(msg.hasDebug());
    }

    SECTION("fails to set a data chunk with unknown descriptor") {
        auto data_chunk_bad = data_chunk;
        data_chunk_bad.descriptor = 0x09;
        REQUIRE_THROWS_AS(msg.setDataChunk(data_chunk_bad),
                          message_processing_error);
    }

    SECTION("fails to set a data chunk with wrong size") {
        auto data_chunk_bad = data_chunk;
        data_chunk_bad.size = 99;
        REQUIRE_THROWS_AS(msg.setDataChunk(data_chunk_bad),
                          message_processing_error);
    }

    SECTION("fails to set a debug chunk with unknown descriptor") {
        auto debug_chunk_bad = data_chunk;
        debug_chunk_bad.descriptor = 0x09;
        REQUIRE_THROWS_AS(msg.addDebugChunk(debug_chunk_bad),
                          message_processing_error);
    }

    SECTION("fails to set a debug chunk with wrong size") {
        auto debug_chunk_bad = data_chunk;
        debug_chunk_bad.size = 99;
        REQUIRE_THROWS_AS(msg.addDebugChunk(debug_chunk_bad),
                          message_processing_error);
    }
}

static const MessageChunk e_c { 0x01, 6, "header" };
static const MessageChunk da_c { 0x02, 5, "stuff" };
static const MessageChunk db_c_1 { 0x03, 6, "errors" };
static const MessageChunk db_c_2 { 0x13, 5, "stats" };

TEST_CASE("Message::getSerialized", "[message]") {
    Message msg { e_c };
    auto expected_buffer = msg_buffer_valid;  // with only version and envelope

    SECTION("serialize correctly a message with only the envelope") {
        auto s_m = msg.getSerialized();
        REQUIRE(s_m == expected_buffer);
    }

    SECTION("serialize correctly a message with data") {
        msg.setDataChunk(da_c);
        auto s_m = msg.getSerialized();

        expected_buffer.insert(expected_buffer.end(),
                               data_chunk_buffer_valid.begin(),
                               data_chunk_buffer_valid.end());

        REQUIRE(s_m == expected_buffer);
    }

    SECTION("serialize correctly a message with a debug chunk") {
        msg.addDebugChunk(db_c_1);
        auto s_m = msg.getSerialized();

        expected_buffer.insert(expected_buffer.end(),
                               debug_chunk_buffer_valid_1.begin(),
                               debug_chunk_buffer_valid_1.end());

        REQUIRE(s_m == expected_buffer);
    }

    SECTION("serialize correctly a message with multiple debug chunks") {
        msg.addDebugChunk(db_c_1);
        msg.addDebugChunk(db_c_2);
        auto s_m = msg.getSerialized();

        expected_buffer.insert(expected_buffer.end(),
                               debug_chunk_buffer_valid_1.begin(),
                               debug_chunk_buffer_valid_1.end());
        expected_buffer.insert(expected_buffer.end(),
                               debug_chunk_buffer_valid_2.begin(),
                               debug_chunk_buffer_valid_2.end());

        REQUIRE(s_m == expected_buffer);
    }

    SECTION("serialize correctly a message with data and debug chunks") {
        msg.setDataChunk(da_c);
        msg.addDebugChunk(db_c_1);
        msg.addDebugChunk(db_c_2);
        auto s_m = msg.getSerialized();

        expected_buffer.insert(expected_buffer.end(),
                               data_chunk_buffer_valid.begin(),
                               data_chunk_buffer_valid.end());
        expected_buffer.insert(expected_buffer.end(),
                               debug_chunk_buffer_valid_1.begin(),
                               debug_chunk_buffer_valid_1.end());
        expected_buffer.insert(expected_buffer.end(),
                               debug_chunk_buffer_valid_2.begin(),
                               debug_chunk_buffer_valid_2.end());

        REQUIRE(s_m == expected_buffer);
    }

    SECTION("can serialize and parse the same content") {
        msg.setDataChunk(da_c);
        auto s_m = msg.getSerialized();
        Message msg_parsed { std::string(s_m.begin(), s_m.end()) };

        REQUIRE(msg_parsed.getEnvelopeChunk() == e_c);
        REQUIRE(msg_parsed.getDataChunk() == da_c);
    }
}

//
// Performance
//

TEST_CASE("Message serialization and parsing performance", "[message]") {
    static const std::string big_txt {
        "a 1019 bytes txt:\n"
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Aliquam "
        "vitae laoreet est. Mauris eget imperdiet turpis. Duis dignissim "
        "sagittis tortor eu molestie. Donec luctus non urna in fringilla. "
        "Curabitur eu ullamcorper diam. Nulla nec tincidunt sem, quis aliquam "
        "purus. Lorem ipsum dolor sit amet, consectetur adipiscing elit. Nam "
        "vel arcu venenatis mauris porttitor sollicitudin. Pellentesque sit "
        "amet purus a leo euismod varius. Nulla consectetur dui sit amet augue "
        "tempus, ac interdum dui tincidunt.\n"
        "Vestibulum nec enim varius, sagittis nisi vel, convallis dolor. Donec "
        "ut sodales magna. Vivamus mi purus, porttitor at magna vel, ornare "
        "faucibus ipsum. Proin augue sapien, ornare nec facilisis a, pretium "
        "vel leo. Ut quam velit, hendrerit sed facilisis id, euismod eget "
        "lorem. Nam congue vitae diam vehicula bibendum. Aenean pulvinar odio "
        "ipsum, quis malesuada dui feugiat vitae. Vivamus leo ligula, luctus "
        "interdum quam quis, tempor pellentesque arcu. Nullam iaculis pulvinar "
        "odio, eget cras amet."
    };

    static const MessageChunk big_envelope { 0x01, big_txt };
    static const MessageChunk big_data { 0x02, big_txt };
    static const MessageChunk big_debug { 0x03, big_txt };

    static const Message big_msg { big_envelope, big_data, big_debug };
    static const auto big_msg_serialized = big_msg.getSerialized();
    static const std::string big_raw_message { big_msg_serialized.begin(),
                                               big_msg_serialized.end() };

    static std::string current_test {};
    static auto num_msg = 10000;

    auto start = std::chrono::high_resolution_clock::now();

    SECTION("serialize " + std::to_string(num_msg) + " messages with 3 chunks "
            "of 1024 bytes") {
        current_test = "serialize";
        size_t expected_size { 1 + 3 * 1024 };  // version byte + 3 chunks

        for (auto idx = 0; idx < num_msg; idx++) {
            auto s_m = big_msg.getSerialized();

            if (s_m.size() != expected_size) {
                FAIL("serialization failure");
            }
        }
    }

    SECTION("parse " + std::to_string(num_msg) + " messages with 3 chunks "
            "of 1024 bytes") {
        current_test = "parse";
        auto big_txt_size = big_txt.size();

        for (auto idx = 0; idx < num_msg; idx++) {
            Message parsed_msg { big_raw_message };

            auto envelope = parsed_msg.getEnvelopeChunk();
            auto data     = parsed_msg.getDataChunk();
            auto debug    = parsed_msg.getDebugChunks();

            if (envelope.content.size() != big_txt_size
                || data.content.size() != big_txt_size
                || debug[0].content.size() != big_txt_size) {
                FAIL("parsing failure");
            }
        }
    }

    auto execution_time =
        static_cast<double>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::high_resolution_clock::now() - start)
                    .count());

    std::cout << "  time to " << current_test << " " << num_msg
              << " messages of " << big_msg_serialized.size() << " bytes: "
              << execution_time / (1000 * 1000) << " s ("
              << static_cast<int>((num_msg / execution_time) * (1000 * 1000))
              << " msg/s)\n";
}

}  // namespace CthunAgent
