#include "root_path.hpp"
#include "../common/content_format.hpp"

#include <pxp-agent/external_module.hpp>
#include <pxp-agent/module_type.hpp>
#include <pxp-agent/configuration.hpp>
#include <pxp-agent/util/process.hpp>

#include <cpp-pcp-client/protocol/chunks.hpp>       // ParsedChunks

#include <leatherman/json_container/json_container.hpp>
#include <leatherman/util/scope_exit.hpp>
#include <leatherman/file_util/file.hpp>

#include <boost/filesystem/operations.hpp>

#include <catch.hpp>

#include <string>
#include <vector>
#include <unistd.h>

#ifdef _WIN32
#define EXTENSION ".bat"
#else
#define EXTENSION ""
#endif

using namespace PXPAgent;

namespace fs = boost::filesystem;
namespace lth_jc = leatherman::json_container;
namespace lth_util = leatherman::util;
namespace lth_file = leatherman::file_util;

static const std::string SPOOL_DIR { std::string { PXP_AGENT_ROOT_PATH }
                                     + "/lib/tests/resources/test_spool" };

static const auto STORAGE = std::make_shared<ResultsStorage>(SPOOL_DIR, "0d");

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
                                       EXTENSION,
                                       STORAGE));
    }

    SECTION("all actions are successfully loaded from a valid external module") {
        ExternalModule mod { PXP_AGENT_ROOT_PATH
                             "/lib/tests/resources/modules/failures_test"
                             EXTENSION,
                             STORAGE };
        REQUIRE(mod.actions.size() == 2u);
    }

    SECTION("throw a Module::LoadingError in case the module has an invalid "
            "metadata schema") {
        REQUIRE_THROWS_AS(
            ExternalModule(PXP_AGENT_ROOT_PATH
                           "/lib/tests/resources/modules_broken/reverse_broken"
                           EXTENSION,
                           STORAGE),
            Module::LoadingError);
    }
}

TEST_CASE("ExternalModule::type", "[modules]") {
    ExternalModule mod { PXP_AGENT_ROOT_PATH
                         "/lib/tests/resources/modules/reverse_valid"
                         EXTENSION,
                         STORAGE };

    SECTION("correctly reports its type") {
        REQUIRE(mod.type() == ModuleType::External);
    }
}

TEST_CASE("ExternalModule::hasAction", "[modules]") {
    ExternalModule mod { PXP_AGENT_ROOT_PATH
                         "/lib/tests/resources/modules/reverse_valid"
                         EXTENSION,
                         STORAGE };

    SECTION("correctly reports false") {
        REQUIRE(!mod.hasAction("foo"));
    }

    SECTION("correctly reports true") {
        REQUIRE(mod.hasAction("string"));
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
                                        EXTENSION,
                                        STORAGE };

        SECTION("correctly call the reverse module") {
            ActionRequest request { RequestType::Blocking, CONTENT };
            auto response = reverse_module.executeAction(request);

            REQUIRE(response.output.std_out.find("anodaram") != std::string::npos);
            REQUIRE(response.output.std_err.empty());
        }
    }

    SECTION("it should handle module failures") {
        ExternalModule test_reverse_module { PXP_AGENT_ROOT_PATH
                                             "/lib/tests/resources/modules/failures_test"
                                             EXTENSION,
                                             STORAGE };

        SECTION("mark the results as invalid if the module returns an invalid result") {
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
            auto response = test_reverse_module.executeAction(request);

            REQUIRE_FALSE(response.action_metadata.includes("results"));
            REQUIRE(response.action_metadata.includes("results_are_valid"));
            REQUIRE_FALSE(response.action_metadata.get<bool>("results_are_valid"));
        }

        SECTION("it should include error output in response") {
            std::string failure_txt { (DATA_FORMAT % "\"1234987\""
                                                   % "\"failures_test\""
                                                   % "\"broken_action\""
                                                   % "\"maradona\"").str() };
            PCPClient::ParsedChunks failure_content {
                    lth_jc::JsonContainer(ENVELOPE_TXT),
                    lth_jc::JsonContainer(failure_txt),
                    NO_DEBUG,
                    0 };
            ActionRequest request { RequestType::Blocking, failure_content };
            auto response = test_reverse_module.executeAction(request);

            REQUIRE(response.action_metadata.includes("results"));
            REQUIRE(response.action_metadata.get<std::string>("results").empty());
            REQUIRE(response.action_metadata.includes("results_are_valid"));
            REQUIRE_FALSE(response.action_metadata.get<bool>("results_are_valid"));
            REQUIRE(response.action_metadata.includes("execution_error"));
            REQUIRE(response.action_metadata.get<std::string>("execution_error").find("we failed, sorry ☹") != std::string::npos);
            REQUIRE(response.output.std_out.empty());
            REQUIRE(response.output.std_err.find("we failed, sorry ☹") != std::string::npos);
        }
    }
}

TEST_CASE("ExternalModule::callAction - non blocking", "[modules]") {
    configureTest();
    lth_util::scope_exit config_cleaner { resetTest };

    SECTION("the pid is written to file") {
        ExternalModule e_m { PXP_AGENT_ROOT_PATH
                             "/lib/tests/resources/modules/reverse_valid"
                             EXTENSION,
                             STORAGE };
        ActionRequest request { RequestType::NonBlocking, NON_BLOCKING_CONTENT };
        fs::path spool_path { SPOOL_DIR };
        auto results_dir = (spool_path / request.transactionId()).string();
        fs::create_directories(results_dir);
        request.setResultsDir(results_dir);
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

TEST_CASE("ExternalModule::getModuleMetadata", "[modules][metadata]") {
    configureTest();
    lth_util::scope_exit config_cleaner { resetTest };

    SECTION("gets a list of actions when module returns valid metadata") {
        try {
            ExternalModule e_m { PXP_AGENT_ROOT_PATH
                                 "/lib/tests/resources/modules/reverse_valid"
                                 EXTENSION,
                                 STORAGE };
            std::vector<std::string> expected_actions { "string", "hash" };
            REQUIRE(e_m.actions == expected_actions);
        } catch (const std::exception& e) {
            FAIL(std::string { "failed to initialize: " } + e.what());
        }
    }

    SECTION("throws Module::LoadingError when module returns no metadata") {
        REQUIRE_THROWS_AS(
            ExternalModule(PXP_AGENT_ROOT_PATH
                           "/lib/tests/resources/broken_modules/reverse_no_metadata"
                           EXTENSION,
                           STORAGE),
            Module::LoadingError);
    }

    SECTION("throws Module::LoadingError when module returns invalid JSON") {
        REQUIRE_THROWS_AS(
            ExternalModule(PXP_AGENT_ROOT_PATH
                           "/lib/tests/resources/broken_modules/reverse_bad_json_format"
                           EXTENSION,
                           STORAGE),
            Module::LoadingError);
    }

    SECTION("throws Module::LoadingError when module returns invalid metadata") {
        REQUIRE_THROWS_AS(
            ExternalModule(PXP_AGENT_ROOT_PATH
                           "/lib/tests/resources/broken_modules/reverse_broken"
                           EXTENSION,
                           STORAGE),
            Module::LoadingError);
    }
}

TEST_CASE("ExternalModule::validateConfiguration", "[modules][configuration]") {
    configureTest();
    lth_util::scope_exit config_cleaner { resetTest };

    SECTION("succeeds when presented with a valid configuration") {
        ExternalModule e_m {
            PXP_AGENT_ROOT_PATH
            "/lib/tests/resources/modules/convert_test"
            EXTENSION,
            lth_jc::JsonContainer(
                "{ \"rate\" : 1.1, \"fee_percent\" : 0.1, \"fee_max\" : 10 }"),
            STORAGE };

        REQUIRE_NOTHROW(e_m.validateConfiguration());
    }

    SECTION("throws exception when presented with an invalid configuration") {
        ExternalModule e_m {
            PXP_AGENT_ROOT_PATH
            "/lib/tests/resources/modules/convert_test"
            EXTENSION,
            lth_jc::JsonContainer(
                "{ \"rate\" : 1.1, \"fee_percent\" : 0.1, \"foo_max\" : 10 }"),
            STORAGE };

        REQUIRE_THROWS_AS(e_m.validateConfiguration(),
                          PCPClient::validation_error);
    }
}

TEST_CASE("ExternalModule::executeAction", "[modules][output]") {
    configureTest();
    lth_util::scope_exit config_cleaner { resetTest };

    SECTION("gets the action output") {
        ExternalModule e_m {
            PXP_AGENT_ROOT_PATH
            "/lib/tests/resources/modules/convert_test"
            EXTENSION,
            lth_jc::JsonContainer(
                "{ \"rate\" : 1.1, \"fee_percent\" : 0.1, \"fee_max\" : 10 }"),
            STORAGE };
        auto convert_txt = (DATA_FORMAT % "\"0632\""
                                        % "\"convert_test\""
                                        % "\"convert\""
                                        % "{\"amount\" : 1000}").str();
        PCPClient::ParsedChunks convert_content {
            lth_jc::JsonContainer(ENVELOPE_TXT),
            lth_jc::JsonContainer(convert_txt),
            NO_DEBUG,
            0 };
        ActionRequest request { RequestType::Blocking, convert_content };

        try {
            REQUIRE(e_m.executeAction(request)
                        .action_metadata
                            .get<double>({ "results", "amount" }) == 1098.9);
        } catch (...) {
            FAIL("failed to execute the action");
        }
    }

    SECTION("validates the action output") {
        ExternalModule e_m {
            PXP_AGENT_ROOT_PATH
            "/lib/tests/resources/modules/convert_test"
            EXTENSION,
            lth_jc::JsonContainer(
                "{ \"rate\" : 1.1, \"fee_percent\" : 0.1, \"fee_max\" : 10 }"),
            STORAGE };
        auto convert_txt = (DATA_FORMAT % "\"0633\""
                                        % "\"convert_test\""
                                        % "\"convert2\""
                                        % "{\"amount\" : 10000}").str();
        PCPClient::ParsedChunks convert_content {
            lth_jc::JsonContainer(ENVELOPE_TXT),
            lth_jc::JsonContainer(convert_txt),
            NO_DEBUG,
            0 };
        ActionRequest request { RequestType::Blocking, convert_content };

        auto response = e_m.executeAction(request);

        REQUIRE(response.action_metadata.includes("results_are_valid"));
        REQUIRE_FALSE(response.action_metadata.get<bool>("results_are_valid"));
    }
}
