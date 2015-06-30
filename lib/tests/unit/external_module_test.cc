#include "root_path.hpp"

#include <cthun-agent/errors.hpp>
#include <cthun-agent/external_module.hpp>
#include <cthun-agent/uuid.hpp>
#include <cthun-agent/file_utils.hpp>

#include <cthun-client/protocol/chunks.hpp>       // ParsedChunks

#include <leatherman/json_container/json_container.hpp>

#include <boost/filesystem/operations.hpp>
#include <boost/format.hpp>

#include <catch.hpp>

#include <string>
#include <vector>
#include <unistd.h>

namespace CthunAgent {

namespace LTH_JC = leatherman::json_container;

const std::string RESULTS_ROOT_DIR { "/tmp/cthun-agent" };
const std::string STRING_ACTION { "string" };

boost::format data_format {
    "{  \"module\" : %1%,"
    "   \"action\" : %2%,"
    "   \"params\" : %3%"
    "}"
};

const std::string reverse_txt { (data_format % "\"reverse\""
                                             % "\"string\""
                                             % "\"maradona\"").str() };

static const std::vector<LTH_JC::JsonContainer> no_debug {};

static const CthunClient::ParsedChunks content {
                    LTH_JC::JsonContainer(),             // envelope
                    LTH_JC::JsonContainer(reverse_txt),  // data
                    no_debug,   // debug
                    0 };        // num invalid debug chunks

TEST_CASE("ExternalModule::ExternalModule", "[modules]") {
    SECTION("can successfully instantiate from a valid external module") {
        REQUIRE_NOTHROW(ExternalModule(std::string { CTHUN_AGENT_ROOT_PATH }
            + "/lib/tests/resources/modules/reverse_valid"));
    }

    SECTION("all actions are successfully loaded from a valid external module") {
        ExternalModule mod { std::string { CTHUN_AGENT_ROOT_PATH }
                             + "/lib/tests/resources/modules/reverse_valid" };
        REQUIRE(mod.actions.size() == 3);
    }

    SECTION("throw an error in case the module has an invalid metadata schema") {
        REQUIRE_THROWS_AS(
            ExternalModule(std::string { CTHUN_AGENT_ROOT_PATH }
                           + "/lib/tests/resources/broken_modules/reverse_broken"),
            module_error);
    }
}

TEST_CASE("ExternalModule::hasAction", "[modules]") {
    ExternalModule mod { std::string { CTHUN_AGENT_ROOT_PATH }
                         + "/lib/tests/resources/modules/reverse_valid" };

    SECTION("correctly reports false") {
        REQUIRE(!mod.hasAction("foo"));
    }

    SECTION("correctly reports true") {
        REQUIRE(mod.hasAction("string"));
    }
}

TEST_CASE("ExternalModule::callAction - blocking", "[modules]") {
    SECTION("the shipped 'reverse' module works correctly") {
        ExternalModule reverse_module { std::string { CTHUN_AGENT_ROOT_PATH }
                                        + "/modules/reverse" };

        SECTION("correctly call the shipped reverse module") {
            ActionRequest request { RequestType::Blocking, content };
            auto outcome = reverse_module.executeAction(request);

            REQUIRE(outcome.stdout.find("anodaram") != std::string::npos);
        }
    }

    SECTION("it should handle module failures") {
        ExternalModule test_reverse_module {
            std::string { CTHUN_AGENT_ROOT_PATH }
            + "/lib/tests/resources/modules/failures_test" };

        SECTION("throw a request_processing_error if the module returns an "
                "invalid result") {
            std::string failure_txt { (data_format % "\"failures_test\""
                                                   % "\"get_an_invalid_result\""
                                                   % "\"maradona\"").str() };
            CthunClient::ParsedChunks failure_content {
                    LTH_JC::JsonContainer(),
                    LTH_JC::JsonContainer(failure_txt),
                    no_debug,
                    0 };
            ActionRequest request { RequestType::Blocking, failure_content };

            REQUIRE_THROWS_AS(test_reverse_module.executeAction(request),
                              request_processing_error);
        }

        SECTION("throw a request_processing_error if a blocking action throws "
                "an exception") {
            std::string failure_txt { (data_format % "\"failures_test\""
                                                   % "\"broken_action\""
                                                   % "\"maradona\"").str() };
            CthunClient::ParsedChunks failure_content {
                    LTH_JC::JsonContainer(),
                    LTH_JC::JsonContainer(failure_txt),
                    no_debug,
                    0 };
            ActionRequest request { RequestType::Blocking, failure_content };

            REQUIRE_THROWS_AS(test_reverse_module.executeAction(request),
                              request_processing_error);
        }
    }
}

}  // namespace CthunAgent
