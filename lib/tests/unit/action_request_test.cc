#include <cthun-agent/action_request.hpp>

#include <cthun-client/protocol/chunks.hpp>

#include <leatherman/json_container/json_container.hpp>

#include <catch.hpp>

#include <vector>

namespace CthunAgent {

namespace lth_jc = leatherman::json_container;

static std::string valid_envelope_txt {
    " { \"id\" : \"123456\","
    "   \"message_type\" : \"test_test_test\","
    "   \"expires\" : \"2015-06-26T22:57:09Z\","
    "   \"targets\" : [\"cth://agent/test_agent\"],"
    "   \"sender\" : \"cth://controller/test_controller\","
    "   \"destination_report\" : false"
    " }" };

static std::string valid_data_txt { "{ \"outcome\" : \"response string\" }" };

TEST_CASE("ActionRequest::ActionRequest", "[request]") {
    lth_jc::JsonContainer envelope { valid_envelope_txt };
    lth_jc::JsonContainer data { valid_data_txt };
    std::vector<lth_jc::JsonContainer> debug {};

    SECTION("successfully instantiates with valid arguments") {
        const CthunClient::ParsedChunks p_c { envelope, data, debug, 0 };

        REQUIRE_NOTHROW(ActionRequest(RequestType::Blocking, p_c));
    }

    SECTION("throw a ActionRequest::Error if no data") {
        const CthunClient::ParsedChunks p_c { envelope, debug, 0 };

        REQUIRE_THROWS_AS(ActionRequest(RequestType::Blocking, p_c),
                          ActionRequest::Error);
    }

    SECTION("throw a ActionRequest::Error if binary data") {
        const CthunClient::ParsedChunks p_c { envelope, "bin data", debug, 0 };

        REQUIRE_THROWS_AS(ActionRequest(RequestType::Blocking, p_c),
                          ActionRequest::Error);
    }

    SECTION("throw a ActionRequest::Error if invalid data") {
        const CthunClient::ParsedChunks p_c { envelope, false, debug, 0 };

        REQUIRE_THROWS_AS(ActionRequest(RequestType::Blocking, p_c),
                          ActionRequest::Error);
    }
}

static std::string rpc_data_txt {
    " { \"transaction_id\" : \"42\","
    "   \"module\" : \"test_module\","
    "   \"action\" : \"test_action\","
    "   \"params\" : { \"path\" : \"/tmp\","
    "                  \"urgent\" : true }"
    " }" };

TEST_CASE("ActionRequest getters", "[request]") {
    lth_jc::JsonContainer envelope { valid_envelope_txt };
    lth_jc::JsonContainer data { rpc_data_txt };
    std::vector<lth_jc::JsonContainer> debug {};

    SECTION("successfully get values") {
        const CthunClient::ParsedChunks p_c { envelope, data, debug, 0 };
        ActionRequest a_r { RequestType::Blocking, p_c };

        SECTION("parsedChunks") {
            // NB: JsonContainer does not define == operation
            REQUIRE(a_r.parsedChunks().envelope.toString()
                    == p_c.envelope.toString());
            REQUIRE(a_r.parsedChunks().data.toString()
                    == p_c.data.toString());
        }

        SECTION("type") {
            REQUIRE(a_r.type() == RequestType::Blocking);
        }

        SECTION("id") {
            REQUIRE(a_r.id() == "123456");
        }

        SECTION("sender") {
            REQUIRE(a_r.sender() == "cth://controller/test_controller");
        }

        SECTION("transactionId") {
            REQUIRE(a_r.transactionId() == "42");
        }

        SECTION("module") {
            REQUIRE(a_r.module() == "test_module");
        }

        SECTION("action") {
            REQUIRE(a_r.action() == "test_action");
        }

        const lth_jc::JsonContainer params { "{ \"path\" : \"/tmp\","
                                             "  \"urgent\" : true }" };

        SECTION("params") {
            REQUIRE(a_r.params().toString() == params.toString());
        }

        SECTION("paramsTxt") {
            REQUIRE(a_r.paramsTxt() == params.toString());
        }
    }
}

}  // namespace CthunAgent
