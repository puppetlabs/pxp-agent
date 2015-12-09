#include "root_path.hpp"
#include "content_format.hpp"

#include <pxp-agent/external_module.hpp>
#include <pxp-agent/configuration.hpp>
#include <pxp-agent/util/process.hpp>

#include <cpp-pcp-client/protocol/chunks.hpp>       // ParsedChunks

#include <leatherman/json_container/json_container.hpp>
#include <leatherman/util/scope_exit.hpp>
#include <leatherman/file_util/file.hpp>

#include <boost/filesystem/operations.hpp>

#include <horsewhisperer/horsewhisperer.h>

#include <catch.hpp>

#include <string>
#include <vector>
#include <unistd.h>

#ifdef _WIN32
#define EXTENSION ".bat"
#else
#define EXTENSION ""
#endif

namespace PXPAgent {

namespace fs = boost::filesystem;
namespace HW = HorseWhisperer;
namespace lth_jc = leatherman::json_container;
namespace lth_util = leatherman::util;
namespace lth_file = leatherman::file_util;

static const std::string SPOOL_DIR { std::string { PXP_AGENT_ROOT_PATH }
                                     + "/lib/tests/resources/test_spool" };

static const std::string REVERSE_TXT {
    (DATA_FORMAT % "\"0987\""
                 % "\"reverse\""
                 % "\"string\""
                 % "{\"argument\" : \"maradona\"}").str() };

static const std::string NON_BLOCKING_REVERSE_TXT {
    (NON_BLOCKING_DATA_FORMAT % "\"1988\""
                              % "\"reverse\""
                              % "\"string\""
                              % "{\"argument\" : \"zico\"}"
                              % "false").str() };

static const std::vector<lth_jc::JsonContainer> NO_DEBUG {};

static const PCPClient::ParsedChunks CONTENT {
                    lth_jc::JsonContainer(ENVELOPE_TXT),  // envelope
                    lth_jc::JsonContainer(REVERSE_TXT),   // data
                    NO_DEBUG,   // debug
                    0 };        // num invalid debug chunks

static const PCPClient::ParsedChunks NON_BLOCKING_CONTENT {
                    lth_jc::JsonContainer(ENVELOPE_TXT),              // envelope
                    lth_jc::JsonContainer(NON_BLOCKING_REVERSE_TXT),  // data
                    NO_DEBUG,   // debug
                    0 };        // num invalid debug chunks

TEST_CASE("ExternalModule::ExternalModule", "[modules]") {
    SECTION("can successfully instantiate from a valid external module") {
        REQUIRE_NOTHROW(ExternalModule(PXP_AGENT_ROOT_PATH
                                       "/lib/tests/resources/modules/reverse_valid"
                                       EXTENSION));
    }

    SECTION("all actions are successfully loaded from a valid external module") {
        ExternalModule mod { PXP_AGENT_ROOT_PATH
                             "/lib/tests/resources/modules/failures_test"
                             EXTENSION };
        REQUIRE(mod.actions.size() == 2u);
    }

    SECTION("throw a Module::LoadingError in case the module has an invalid "
            "metadata schema") {
        REQUIRE_THROWS_AS(
            ExternalModule(PXP_AGENT_ROOT_PATH
                           "/lib/tests/resources/broken_modules/reverse_broken"
                           EXTENSION),
            Module::LoadingError);
    }
}

TEST_CASE("ExternalModule::type", "[modules]") {
    ExternalModule mod { PXP_AGENT_ROOT_PATH
                         "/lib/tests/resources/modules/reverse_valid"
                         EXTENSION };

    SECTION("correctly reports its type") {
        REQUIRE(mod.type() == Module::Type::External);
    }
}

TEST_CASE("ExternalModule::hasAction", "[modules]") {
    ExternalModule mod { PXP_AGENT_ROOT_PATH
                         "/lib/tests/resources/modules/reverse_valid"
                         EXTENSION };

    SECTION("correctly reports false") {
        REQUIRE(!mod.hasAction("foo"));
    }

    SECTION("correctly reports true") {
        REQUIRE(mod.hasAction("string"));
    }
}

TEST_CASE("ExternalModule::readNonBlockingOutcome", "[modules]") {
    ActionRequest request { RequestType::NonBlocking, NON_BLOCKING_CONTENT };
    fs::path resources_path { PXP_AGENT_ROOT_PATH "/lib/tests/resources" };
    std::string o;
    std::string e;

    SECTION("it does read out") {
        auto o_f = (resources_path / "delayed_result_success/stdout").string();
        auto e_f = (resources_path / "delayed_result_success/stderr").string();
        ExternalModule::readNonBlockingOutcome(request, o_f, e_f, o, e);

        REQUIRE(o == "***OUTPUT\n");
        REQUIRE(e.empty());
    }

    SECTION("it does read out and err") {
        auto o_f = (resources_path / "delayed_result_success/stdout").string();
        auto e_f = (resources_path / "delayed_result_failure/stderr").string();
        ExternalModule::readNonBlockingOutcome(request, o_f, e_f, o, e);

        REQUIRE(o == "***OUTPUT\n");
        REQUIRE(e == "***ERROR\n");
    }
}

static void configureTest() {
    if (!fs::exists(SPOOL_DIR) && !fs::create_directories(SPOOL_DIR)) {
        FAIL("Failed to create the results directory");
    }
    Configuration::Instance().initialize(
        [](std::vector<std::string>) {
            return EXIT_SUCCESS;
        });
    HW::SetFlag<std::string>("spool-dir", SPOOL_DIR);
}

static void resetTest() {
    if (fs::exists(SPOOL_DIR)) {
        fs::remove_all(SPOOL_DIR);
    }
}

TEST_CASE("ExternalModule::callAction - blocking", "[modules]") {
    configureTest();
    lth_util::scope_exit config_cleaner { resetTest };

    SECTION("the shipped 'reverse' module works correctly") {
        ExternalModule reverse_module { PXP_AGENT_ROOT_PATH
                                        "/lib/tests/resources//modules/reverse_valid"
                                        EXTENSION };

        SECTION("correctly call the reverse module") {
            ActionRequest request { RequestType::Blocking, CONTENT };
            auto outcome = reverse_module.executeAction(request);

            REQUIRE(outcome.std_out.find("anodaram") != std::string::npos);
        }
    }

    SECTION("it should handle module failures") {
        ExternalModule test_reverse_module { PXP_AGENT_ROOT_PATH
                                             "/lib/tests/resources/modules/failures_test"
                                             EXTENSION };

        SECTION("throw a Module::ProcessingError if the module returns an "
                "invalid result") {
            std::string failure_txt { (DATA_FORMAT % "\"1234987\""
                                                   % "\"failures_test\""
                                                   % "\"get_an_invalid_result\""
                                                   % "\"maradona\"").str() };
            PCPClient::ParsedChunks failure_content {
                    lth_jc::JsonContainer(ENVELOPE_TXT),
                    lth_jc::JsonContainer(failure_txt),
                    NO_DEBUG,
                    0 };
            ActionRequest request { RequestType::Blocking, failure_content };

            REQUIRE_THROWS_AS(test_reverse_module.executeAction(request),
                              Module::ProcessingError);
        }

        SECTION("throw a Module::ProcessingError if a blocking action throws "
                "an exception") {
            std::string failure_txt { (DATA_FORMAT % "\"43217890\""
                                                   % "\"failures_test\""
                                                   % "\"broken_action\""
                                                   % "\"maradona\"").str() };
            PCPClient::ParsedChunks failure_content {
                    lth_jc::JsonContainer(ENVELOPE_TXT),
                    lth_jc::JsonContainer(failure_txt),
                    NO_DEBUG,
                    0 };
            ActionRequest request { RequestType::Blocking, failure_content };

            REQUIRE_THROWS_AS(test_reverse_module.executeAction(request),
                              Module::ProcessingError);
        }
    }
}

TEST_CASE("ExternalModule::callAction - non blocking", "[modules]") {
    configureTest();
    lth_util::scope_exit config_cleaner { resetTest };

    SECTION("the pid is written to file") {
        ExternalModule e_m { PXP_AGENT_ROOT_PATH
                             "/lib/tests/resources/modules/reverse_valid"
                             EXTENSION };
        ActionRequest request { RequestType::NonBlocking, NON_BLOCKING_CONTENT };
        fs::path spool_path { SPOOL_DIR };
        fs::create_directories(spool_path / request.transactionId());
        auto pid_path = spool_path / request.transactionId() / "pid";

        REQUIRE_NOTHROW(e_m.executeAction(request));
        REQUIRE(fs::exists(pid_path));

        try {
            auto pid_txt = lth_file::read(pid_path.string());
            auto pid = std::stoi(pid_txt);
        } catch (std::exception) {
            FAIL("fail to get pid");
        }
    }
}

}  // namespace PXPAgent
