#include "../common/content_format.hpp"

#include <pxp-agent/action_request.hpp>

#include <leatherman/json_container/json_container.hpp>

#include <catch.hpp>

#include <vector>

namespace PXPAgent {

namespace lth_jc = leatherman::json_container;

static const std::string DATA_TXT {
    (DATA_FORMAT % "\"04352987\""
                 % "\"module name\""
                 % "\"action name\""
                 % "{ \"some key\" : \"some value\" }").str() };

TEST_CASE("ActionRequest::ActionRequest", "[request]") {
    lth_jc::JsonContainer data { DATA_TXT };

    SECTION("successfully instantiates with valid arguments") {
        REQUIRE_NOTHROW(ActionRequest(RequestType::Blocking, MESSAGE_ID, SENDER, data));
    }
}

static const std::string pxp_data_txt {
    (DATA_FORMAT % "\"42\""
                 % "\"test_module\""
                 % "\"test_action\""
                 % "{ \"path\" : \"/tmp\", \"urgent\" : true }").str() };

TEST_CASE("ActionRequest getters", "[request]") {
    lth_jc::JsonContainer data { pxp_data_txt };

    SECTION("successfully get values") {
        ActionRequest a_r { RequestType::Blocking, MESSAGE_ID, SENDER, data };

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
    lth_jc::JsonContainer data { pxp_data_txt };
    ActionRequest a_r { RequestType::Blocking, MESSAGE_ID, SENDER, data };

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

}  // namespace PXPAgent
