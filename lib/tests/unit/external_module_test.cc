#include "test/test.hpp"
#include <catch.hpp>

#include <cthun-agent/errors.hpp>
#include <cthun-agent/external_module.hpp>
#include <cthun-agent/uuid.hpp>
#include <cthun-agent/file_utils.hpp>
#include <cthun-agent/timer.hpp>

#include <cthun-client/data_container/data_container.hpp>
#include <cthun-client/message/chunks.hpp>       // ParsedChunks

#include <boost/filesystem/operations.hpp>
#include <boost/format.hpp>

#include <string>
#include <vector>
#include <unistd.h>

extern std::string ROOT_PATH;

namespace CthunAgent {

// TODO(ale): update this to cthun-client

const std::string RESULTS_ROOT_DIR { "/tmp/cthun-agent" };
const std::string DELAYED_JOB_ID_LABEL { "id" };
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

static const std::vector<std::string> no_debug {};

static const CthunClient::ParsedChunks content {
                    CthunClient::DataContainer(),
                    CthunClient::DataContainer(reverse_txt),
                    no_debug };

TEST_CASE("ExternalModule::ExternalModule", "[modules]") {
    SECTION("can successfully instantiate from a valid external module") {
        REQUIRE_NOTHROW(ExternalModule(
            ROOT_PATH + "/lib/tests/resources/modules/reverse_valid"));
    }

    SECTION("all actions are successfully loaded from a valid external module") {
        ExternalModule mod { ROOT_PATH + "/lib/tests/resources/modules/reverse_valid" };
        REQUIRE(mod.actions.size() == 3);
    }

    SECTION("the behaviour of an action is correctly stored") {
        ExternalModule mod { ROOT_PATH + "/lib/tests/resources/modules/reverse_valid" };
        REQUIRE(mod.actions["delayed_action"].behaviour == "delayed");
    }

    SECTION("the default behaviour of an action is 'interactive'") {
        ExternalModule mod { ROOT_PATH + "/lib/tests/resources/modules/reverse_valid" };
        REQUIRE(mod.actions[STRING_ACTION].behaviour == "interactive");
    }

    SECTION("throw an error in case the module has an invalid metadata schema") {
        REQUIRE_THROWS_AS(
            ExternalModule(ROOT_PATH + "/lib/tests/resources/modules/reverse_broken"),
            module_error);
    }
}

TEST_CASE("ExternalModule::callAction - blocking", "[modules]") {
    SECTION("the shipped 'reverse' module works correctly") {
        ExternalModule reverse_module { ROOT_PATH + "/modules/reverse" };

        SECTION("correctly call the shipped reverse module") {
            auto result = reverse_module.performRequest(STRING_ACTION,
                                                               content);
            REQUIRE(result.toString().find("anodaram") != std::string::npos);
        }

        SECTION("throw a request_validation_error if the action is unknown") {
            REQUIRE_THROWS_AS(reverse_module.performRequest("fake_action",
                                                                    content),
                              request_validation_error);
        }

        SECTION("throw a request_validation_error if the message is invalid") {
            std::string bad_reverse_txt { (data_format % "\"reverse\""
                                                       % "\"string\""
                                                       % "[1, 2, 3, 4 ,5]").str() };
            CthunClient::ParsedChunks bad_content {
                    CthunClient::DataContainer(),
                    CthunClient::DataContainer(bad_reverse_txt),
                    no_debug };

            REQUIRE_THROWS_AS(reverse_module.performRequest(STRING_ACTION,
                                                            bad_content),
                              request_validation_error);
        }
    }

    SECTION("it should handle module failures") {
        ExternalModule test_reverse_module {
            ROOT_PATH + "/lib/tests/resources/modules/failures_test" };

        SECTION("throw a request_processing_error if the module returns an "
                "invalid result") {
            std::string failure_txt { (data_format % "\"failures_test\""
                                                   % "\"get_an_invalid_result\""
                                                   % "\"maradona\"").str() };
            CthunClient::ParsedChunks failure_content {
                    CthunClient::DataContainer(),
                    CthunClient::DataContainer(failure_txt),
                    no_debug };

            REQUIRE_THROWS_AS(test_reverse_module.performRequest(
                                    "get_an_invalid_result", failure_content),
                              request_processing_error);
        }

        SECTION("throw a request_processing_error if a blocking action throws "
                "an exception") {
            std::string failure_txt { (data_format % "\"failures_test\""
                                                   % "\"broken_action\""
                                                   % "\"maradona\"").str() };
            CthunClient::ParsedChunks failure_content {
                    CthunClient::DataContainer(),
                    CthunClient::DataContainer(failure_txt),
                    no_debug };

            REQUIRE_THROWS_AS(test_reverse_module.performRequest(
                                    "broken_action", failure_content),
                              request_processing_error);
        }
    }
}

void waitForAction(const std::string& action_dir) {
    Timer timer {};
    while (timer.elapsedSeconds() < 2) {  // [s]
        usleep(10000);  // [us]
        if (FileUtils::fileExists(action_dir + "/status")) {
            auto status = FileUtils::readFileAsString(action_dir + "/status");
            if (status.find("completed") != std::string::npos) {
                return;
            }
        }
    }
}

TEST_CASE("ExternalModule::callAction - delayed", "[async]") {
    SECTION("it should return the response data containing the delayed job id") {
        ExternalModule test_reverse_module {
            ROOT_PATH + "/lib/tests/resources/modules/reverse_valid" };
        std::string delayed_txt { (data_format % "\"reverse_valid\""
                                               % "\"delayed_action\""
                                               % "\"the input string\"").str() };
        CthunClient::ParsedChunks delayed_content {
                CthunClient::DataContainer(),
                CthunClient::DataContainer(delayed_txt),
                no_debug };
        auto result = test_reverse_module.performRequest("delayed_action",
                                                         delayed_content);

        REQUIRE(result.includes(DELAYED_JOB_ID_LABEL));

        // Try to clean it up
        std::string action_dir { RESULTS_ROOT_DIR + "/"
                                 + result.get<std::string>(DELAYED_JOB_ID_LABEL) };

        if (FileUtils::fileExists(action_dir)) {
            waitForAction(action_dir);
            boost::filesystem::remove_all(action_dir);
        }
    }
}

TEST_CASE("ExternalModule::executeDelayedAction", "[async]") {
    // NB: we cannot test a delayed action by calling callAction() as
    // done for the blocking ones; we must know the job_id to access
    // the result files so we test executeDelayedAction directly

    ExternalModule test_reverse_module {
        ROOT_PATH + "/lib/tests/resources/modules/reverse_valid" };

    std::string action_parent_dir { RESULTS_ROOT_DIR + "/" };
    auto job_id = UUID::getUUID();
    auto action_dir = action_parent_dir + job_id;

    while (FileUtils::fileExists(action_dir)) {
        job_id =  UUID::getUUID();
        action_dir = action_parent_dir + job_id;
    }

    SECTION("it should create the result directory for a given job") {
        test_reverse_module.executeDelayedAction(STRING_ACTION, content, job_id);
        waitForAction(action_dir);
        CHECK(FileUtils::fileExists(action_dir));
        boost::filesystem::remove_all(action_dir);
    }

    SECTION("it should create the status, stdout, and stderr files") {
        test_reverse_module.executeDelayedAction(STRING_ACTION, content, job_id);
        waitForAction(action_dir);
        CHECK(FileUtils::fileExists(action_dir + "/status"));
        CHECK(FileUtils::fileExists(action_dir + "/stdout"));
        CHECK(FileUtils::fileExists(action_dir + "/stderr"));
        boost::filesystem::remove_all(action_dir);
    }

    SECTION("it should correctly write the completed status") {
        test_reverse_module.executeDelayedAction(STRING_ACTION, content, job_id);
        waitForAction(action_dir);
        CHECK(FileUtils::fileExists(action_dir + "/status"));

        if (FileUtils::fileExists(action_dir + "/status")) {
            auto result = FileUtils::readFileAsString(action_dir + "/status");
            CHECK(result.find("completed") != std::string::npos);
        }

        boost::filesystem::remove_all(action_dir);
    }

    SECTION("it should correctly write the result") {
        test_reverse_module.executeDelayedAction(STRING_ACTION, content, job_id);
        waitForAction(action_dir);
        CHECK(FileUtils::fileExists(action_dir + "/stdout"));

        if (FileUtils::fileExists(action_dir + "/stdout")) {
            auto result = FileUtils::readFileAsString(action_dir + "/stdout");
            CHECK(result.find("anodaram") != std::string::npos);
        }

        boost::filesystem::remove_all(action_dir);
    }
}

}  // namespace CthunAgent
