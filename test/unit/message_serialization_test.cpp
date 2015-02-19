#include "test/test.h"

#include "src/message_serialization.h"

#include <iostream>
#include <string>
#include <stdint.h>

namespace CthunAgent {

TEST_CASE("CthunAgent::serialize", "[message]") {
    SECTION("can serialize a string") {
        std::string s { "lalala" };
        SerializedMessage buffer;
        serialize<std::string>(s, s.size(), buffer);

        REQUIRE(buffer.size() == s.size());
        REQUIRE(buffer == std::vector<uint8_t>({ 'l', 'a', 'l', 'a', 'l', 'a' }));
    }

    SECTION("can serialize a string with spaces") {
        std::string s { "carry on" };
        SerializedMessage buffer;
        serialize<std::string>(s, s.size(), buffer);

        REQUIRE(buffer.size() == s.size());
        REQUIRE(buffer == std::vector<uint8_t>({ 'c', 'a', 'r', 'r', 'y', ' ',
                                                 'o', 'n' }));
    }

    SECTION("can serialize Weißbier!") {
        std::string s { "Weißbier!" };
        SerializedMessage buffer;
        serialize<std::string>(s, s.size(), buffer);

        REQUIRE(std::string(buffer.begin(), buffer.end()) == s);
    }

    SECTION("can serialize multiple strings") {
        std::string s_1 { "Matryoshka " };
        std::string s_2 { "(Матрёшка) " };
        std::string s_3 { "dolls" };
        auto size_1 = s_1.size();
        auto size_2 = s_2.size();
        auto size_3 = s_3.size();

        SerializedMessage buffer;
        serialize<std::string>(s_1, size_1, buffer);
        serialize<std::string>(s_2, size_2, buffer);
        serialize<std::string>(s_3, size_3, buffer);

        REQUIRE(buffer.size() == size_1 + size_2 + size_3);
        REQUIRE(std::string(buffer.begin(), buffer.end()) == s_1 + s_2 + s_3);
    }

    SECTION("can serialize an unsigned long integer (4 bytes)") {
        uint32_t n { 42 };
        SerializedMessage buffer;
        serialize<uint32_t>(n, 4, buffer);

        REQUIRE(buffer.size() == sizeof(uint32_t));
        REQUIRE(buffer == std::vector<uint8_t>({ 0, 0, 0, 0x2A }));
    }

    SECTION("can serialize an unsigned long integer (4 bytes) storing a number "
            "encoded with multiple bytes") {
        uint32_t n { 271828 };
        SerializedMessage buffer;
        serialize<uint32_t>(n, 4, buffer);

        REQUIRE(buffer.size() == sizeof(uint32_t));
        REQUIRE(buffer == std::vector<uint8_t>({ 0, 0x4, 0x25, 0xD4 }));
    }

    SECTION("can serialize multiple long integers") {
        uint32_t n_1 { 271828 };
        uint32_t n_2 { 314159 };

        SerializedMessage buffer;
        serialize<uint32_t>(n_1, 4, buffer);
        serialize<uint32_t>(n_2, 4, buffer);

        REQUIRE(buffer.size() == 2 * sizeof(uint32_t));
        REQUIRE(buffer == std::vector<uint8_t>({ 0, 0x4, 0x25, 0xD4,
                                                 0, 0x4, 0xCB, 0x2F }));
    }

    SECTION("can serialize a short integer (1 byte)") {
        uint8_t n { 42 };
        SerializedMessage buffer;
        serialize<uint8_t>(n, 1, buffer);

        REQUIRE(buffer.size() == sizeof(uint8_t));
        REQUIRE(buffer == std::vector<uint8_t>({ 0x2A }));
    }

    SECTION("can serialize multiple short integers") {
        uint8_t n_1 { 42 };
        uint8_t n_2 { 127 };
        uint8_t n_3 { 4 };

        SerializedMessage buffer;
        serialize<uint8_t>(n_1, 1, buffer);
        serialize<uint8_t>(n_2, 1, buffer);
        serialize<uint8_t>(n_3, 1, buffer);

        REQUIRE(buffer.size() == 3 * sizeof(uint8_t));
        REQUIRE(buffer == std::vector<uint8_t>({ 0x2A, 0x7F, 0x4 }));
    }
}

TEST_CASE("CthunAgent::deserialize", "[message]") {
    SECTION("can deserialize a short integer") {
        // NB: better not use init. lists with vector ctor (S. Meyers)
        auto n_buffer = SerializedMessage({ 42 });

        SerializedMessage::const_iterator next_itr { n_buffer.begin() };
        auto n_deserialized = deserialize<uint8_t>(1, next_itr);

        REQUIRE(n_deserialized == 42);
    }

    SECTION("can deserialize multiple short integers") {
        auto n_buffer = SerializedMessage({ 42, 88, 0, 127 });

        SerializedMessage::const_iterator next_itr { n_buffer.begin() };
        auto n_1 = deserialize<uint8_t>(1, next_itr);
        auto n_2 = deserialize<uint8_t>(1, next_itr);
        auto n_3 = deserialize<uint8_t>(1, next_itr);
        auto n_4 = deserialize<uint8_t>(1, next_itr);

        REQUIRE(next_itr == n_buffer.end());
        REQUIRE(n_1 == 42);
        REQUIRE(n_2 == 88);
        REQUIRE(n_3 == 0);
        REQUIRE(n_4 == 127);
    }

    SECTION("can deserialize a long integer") {
        uint32_t n { 271828 };
        SerializedMessage n_buffer;
        serialize<uint32_t>(n, 4, n_buffer);

        SerializedMessage::const_iterator next_itr { n_buffer.begin() };
        auto n_deserialized = deserialize<uint32_t>(4, next_itr);

        REQUIRE(n_deserialized == 271828);
    }

    SECTION("can deserialize multiple long integers") {
        auto n_vector = std::vector<uint32_t>({ 271828, 0, 1, 128, 1025 });
        SerializedMessage n_buffer;
        for (const auto& n : n_vector)
            serialize(n, 4, n_buffer);

        SerializedMessage::const_iterator next_itr { n_buffer.begin() };
        auto n_1 = deserialize<uint32_t>(4, next_itr);
        auto n_2 = deserialize<uint32_t>(4, next_itr);
        auto n_3 = deserialize<uint32_t>(4, next_itr);
        auto n_4 = deserialize<uint32_t>(4, next_itr);
        auto n_5 = deserialize<uint32_t>(4, next_itr);

        REQUIRE(next_itr == n_buffer.end());
        REQUIRE(n_1 == 271828);
        REQUIRE(n_2 == 0);
        REQUIRE(n_3 == 1);
        REQUIRE(n_4 == 128);
        REQUIRE(n_5 == 1025);
    }

    SECTION("can deserialize a string") {
        std::string s { "one two three!" };
        SerializedMessage s_buffer;
        serialize<std::string>(s, s.size(), s_buffer);

        SerializedMessage::const_iterator next_itr { s_buffer.begin() };
        auto s_deserialized = deserialize<std::string>(s.size(), next_itr);

        REQUIRE(next_itr == s_buffer.end());
        REQUIRE(s_deserialized == s);
    }

    SECTION("can deserialize numbers and strings") {
        uint8_t descriptor { 1 };
        std::string some_data { "this could be a message header" };
        uint32_t data_size { static_cast<uint32_t>(some_data.size()) };

        SerializedMessage buffer;
        serialize<uint8_t>(descriptor, 1, buffer);
        serialize<uint32_t>(data_size, 4, buffer);
        serialize<std::string>(some_data, some_data.size(), buffer);

        SerializedMessage::const_iterator next_itr { buffer.begin() };
        auto descriptor_d = deserialize<uint8_t>(1, next_itr);
        auto data_size_d  = deserialize<uint32_t>(4, next_itr);
        auto some_data_d  = deserialize<std::string>(data_size_d, next_itr);

        REQUIRE(next_itr == buffer.end());
        REQUIRE(descriptor_d == descriptor);
        REQUIRE(data_size_d  == data_size);
        REQUIRE(some_data_d  == some_data);
    }
}

}  // namespace CthunAgent
