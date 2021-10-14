#include "../common/content_format.hpp"

#include <pxp-agent/action_request.hpp>

#include <cpp-pcp-client/protocol/chunks.hpp>

#include <leatherman/json_container/json_container.hpp>

#include <catch.hpp>

#include <vector>

using namespace PXPAgent;

namespace lth_jc = leatherman::json_container;

static const std::string DATA_TXT {
    (DATA_FORMAT % "\"04352987\""
                 % "\"module name\""
                 % "\"action name\""
                 % "{ \"some key\" : \"some value\" }").str() };

TEST_CASE("ActionRequest::ActionRequest", "[request]") {
    lth_jc::JsonContainer envelope { ENVELOPE_TXT };
    lth_jc::JsonContainer data { DATA_TXT };
    std::vector<lth_jc::JsonContainer> debug {};

    SECTION("successfully instantiates with valid arguments") {
        const PCPClient::ParsedChunks p_c { envelope, data, debug, 0 };

        REQUIRE_NOTHROW(ActionRequest(RequestType::Blocking, p_c));
    }

    SECTION("throw a ActionRequest::Error if no data") {
        const PCPClient::ParsedChunks p_c { envelope, debug, 0 };

        REQUIRE_THROWS_AS(ActionRequest(RequestType::Blocking, p_c),
                          ActionRequest::Error);
    }

    SECTION("throw a ActionRequest::Error if binary data") {
        const PCPClient::ParsedChunks p_c { envelope, std::string{"bin data"}, debug, 0 };

        REQUIRE_THROWS_AS(ActionRequest(RequestType::Blocking, p_c),
                          ActionRequest::Error);
    }

    SECTION("throw a ActionRequest::Error if invalid data") {
        const PCPClient::ParsedChunks p_c { envelope, false, debug, 0 };

        REQUIRE_THROWS_AS(ActionRequest(RequestType::Blocking, p_c),
                          ActionRequest::Error);
    }
}

static std::string pxp_data_txt {
    " { \"transaction_id\" : \"42\","
    "   \"module\" : \"test_module\","
    "   \"action\" : \"test_action\","
    "   \"params\" : { \"path\" : \"/tmp\","
    "                  \"urgent\" : true }"
    " }" };

TEST_CASE("ActionRequest getters", "[request]") {
    lth_jc::JsonContainer envelope { ENVELOPE_TXT };
    lth_jc::JsonContainer data { pxp_data_txt };
    std::vector<lth_jc::JsonContainer> debug {};

    SECTION("successfully get values") {
        const PCPClient::ParsedChunks p_c { envelope, data, debug, 0 };
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
            REQUIRE(a_r.sender() == "pcp://controller/test_controller");
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

        SECTION("can call prettyLabel") {
            REQUIRE_NOTHROW(a_r.prettyLabel());
        }
    }
}

TEST_CASE("ActionRequest results_dir setter / getter", "[request]") {
    lth_jc::JsonContainer envelope { ENVELOPE_TXT };
    lth_jc::JsonContainer data { pxp_data_txt };
    std::vector<lth_jc::JsonContainer> debug {};
    const PCPClient::ParsedChunks p_c { envelope, data, debug, 0 };
    ActionRequest a_r { RequestType::Blocking, p_c };

    SECTION("getter returns an empty string without a previous setter call") {
        auto results_dir = a_r.resultsDir();
        REQUIRE(results_dir == std::string());
    }

    SECTION("correctly sets and gets the results_dir on a const instance") {
        std::string results_dir { "/tmp/beans" };
        a_r.setResultsDir(results_dir);
        REQUIRE(a_r.resultsDir() == results_dir);
    }
}
