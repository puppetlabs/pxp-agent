#include "test/test.h"

#include "src/data_parser.h"
#include "src/message.h"
#include "src/string_utils.h"
#include "src/uuid.h"
#include "src/errors.h"
#include "src/schemas.h"

#include <boost/filesystem/operations.hpp>
#include <boost/format.hpp>

#include <iostream>
#include <vector>

namespace CthunAgent {

boost::format envelope_format {
    "{"
    "    \"id\" : \"%1%\","
    "    \"expires\" : \"%2%\","
    "    \"sender\" : \"%3%\","
    "    \"data_schema\" : \"%4%\","
    "    \"endpoints\" : [\"%5%\"]"
    "}"
};

// TODO(ale): add more tests

TEST_CASE("DataParser::parseAndValidateChunk", "[data]") {
    auto expires = StringUtils::getISO8601Time(10);
    auto id = UUID::getUUID();

    SECTION("can parse and validate an envelope") {
        std::string envelope_txt {
            (envelope_format % id
                             % expires
                             % "cth://server"
                             % CTHUN_REQUEST_SCHEMA
                             % "cth://0005_controller/pegasus-controller").str()
        };
        MessageChunk envelope_chunk { ChunkDescriptor::ENVELOPE, envelope_txt };
        REQUIRE_NOTHROW(DataParser::parseAndValidateChunk(envelope_chunk));
    }

    SECTION("fails if the data schema is not the expected request one") {
        for (auto schema : std::vector<std::string>({ CTHUN_LOGIN_SCHEMA,
                                                      CTHUN_RESPONSE_SCHEMA })) {
            std::string envelope_txt {
                (envelope_format % id
                                 % expires
                                 % "cth://server"
                                 % schema
                                 % "cth://0005_controller/pegasus-controller").str()
            };
            MessageChunk envelope_chunk { ChunkDescriptor::ENVELOPE, envelope_txt };
            REQUIRE_THROWS_AS(DataParser::parseAndValidateChunk(envelope_chunk),
                              message_validation_error);
        }
    }
}

}  // namespace CthunAgent
