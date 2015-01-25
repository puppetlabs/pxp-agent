#include "test/test.h"

#include "src/data_container.h"
#include "src/errors.h"
#include "src/external_module.h"
#include "src/uuid.h"
#include "src/file_utils.h"
#include "src/timer.h"

#include <boost/filesystem/operations.hpp>
#include <boost/format.hpp>

#include <vector>
#include <unistd.h>

extern std::string ROOT_PATH;

namespace CthunAgent {

const std::string RESULTS_ROOT_DIR { "/tmp/cthun-agent" };
const std::string DELAYED_JOB_ID_LABEL { "id" };
const std::string STRING_ACTION { "string" };

boost::format msg_format {
    "{  \"data\" : {"
    "       \"module\" : %1%,"
    "       \"action\" : %2%,"
    "       \"params\" : %3%"
    "   }"
    "}"
};

const std::string reverse_txt { (msg_format % "\"reverse\""
                                            % "\"string\""
                                            % "\"maradona\"").str() };
const Message msg { reverse_txt };

TEST_CASE("ExternalModule::ExternalModule", "[modules]") {
    SECTION("can successfully instantiate from a valid external module") {
        REQUIRE_NOTHROW(ExternalModule(
            ROOT_PATH + "/test/resources/modules/reverse_valid"));
    }

    SECTION("all actions are successfully loaded from a valid external module") {
        ExternalModule mod { ROOT_PATH + "/test/resources/modules/reverse_valid" };
        REQUIRE(mod.actions.size() == 3);
    }

    SECTION("the behaviour of an action is correctly stored") {
        ExternalModule mod { ROOT_PATH + "/test/resources/modules/reverse_valid" };
        REQUIRE(mod.actions["delayed_action"].behaviour == "delayed");
    }

    SECTION("the default behaviour of an action is 'interactive'") {
        ExternalModule mod { ROOT_PATH + "/test/resources/modules/reverse_valid" };
        REQUIRE(mod.actions[STRING_ACTION].behaviour == "interactive");
    }

    SECTION("throw an error in case the module has an invalid metadata schema") {
        REQUIRE_THROWS_AS(
            ExternalModule(ROOT_PATH + "/test/resources/modules/reverse_broken"),
            module_error);
    }
}

TEST_CASE("ExternalModule::callAction - blocking", "[modules]") {
    SECTION("the shipped 'reverse' module works correctly") {
        ExternalModule reverse_module { ROOT_PATH + "/modules/reverse" };

        SECTION("correctly call the shipped reverse module") {
            auto result = reverse_module.validateAndCallAction(STRING_ACTION, msg);
            REQUIRE(result.toString().find("anodaram") != std::string::npos);
        }

        SECTION("throw a message_validation_error if the action is unknown") {
            REQUIRE_THROWS_AS(reverse_module.validateAndCallAction("fake_action",
                                                                    msg),
                              message_validation_error);
        }

        SECTION("throw a message_validation_error if the message is invalid") {
            std::string bad_reverse_txt { (msg_format % "\"reverse\""
                                                      % "\"string\""
                                                      % "[1, 2, 3, 4 ,5]").str() };
            Message bad_msg { bad_reverse_txt };
            REQUIRE_THROWS_AS(reverse_module.validateAndCallAction(STRING_ACTION,
                                                                   bad_msg),
                              message_validation_error);
        }
    }

    SECTION("it should handle module failures") {
        ExternalModule test_reverse_module {
            ROOT_PATH + "/test/resources/modules/failures_test" };

        SECTION("throw a message_processing_error if the module returns an "
                "invalid result") {
            std::string failure_txt { (msg_format % "\"failures_test\""
                                                  % "\"get_an_invalid_result\""
                                                  % "\"maradona\"").str() };
            Message failure_msg { failure_txt };
            REQUIRE_THROWS_AS(test_reverse_module.validateAndCallAction(
                                    "get_an_invalid_result", failure_msg),
                              message_processing_error);
        }

        SECTION("throw a message_processing_error if a blocking action throws "
                "an exception") {
            std::string failure_txt { (msg_format % "\"failures_test\""
                                                  % "\"broken_action\""
                                                  % "\"maradona\"").str() };
            Message failure_msg { failure_txt };
            REQUIRE_THROWS_AS(test_reverse_module.validateAndCallAction(
                                    "broken_action", failure_msg),
                              message_processing_error);
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
            ROOT_PATH + "/test/resources/modules/reverse_valid" };
        std::string delayed_txt { (msg_format % "\"reverse_valid\""
                                              % "\"delayed_action\""
                                              % "\"the input string\"").str() };
        Message delayed_msg { delayed_txt };
        auto result = test_reverse_module.validateAndCallAction("delayed_action",
                                                                delayed_msg);
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
        ROOT_PATH + "/test/resources/modules/reverse_valid" };
    std::string action_parent_dir { RESULTS_ROOT_DIR + "/" };
    auto job_id = UUID::getUUID();
    auto action_dir = action_parent_dir + job_id;

    while (FileUtils::fileExists(action_dir)) {
        job_id =  UUID::getUUID();
        action_dir = action_parent_dir + job_id;
    }

    SECTION("it should create the result directory for a given job") {
        test_reverse_module.executeDelayedAction(STRING_ACTION, msg, job_id);
        waitForAction(action_dir);
        CHECK(FileUtils::fileExists(action_dir));
        boost::filesystem::remove_all(action_dir);
    }

    SECTION("it should create the status, stdout, and stderr files") {
        test_reverse_module.executeDelayedAction(STRING_ACTION, msg, job_id);
        waitForAction(action_dir);
        CHECK(FileUtils::fileExists(action_dir + "/status"));
        CHECK(FileUtils::fileExists(action_dir + "/stdout"));
        CHECK(FileUtils::fileExists(action_dir + "/stderr"));
        boost::filesystem::remove_all(action_dir);
    }

    SECTION("it should correctly write the completed status") {
        test_reverse_module.executeDelayedAction(STRING_ACTION, msg, job_id);
        waitForAction(action_dir);
        CHECK(FileUtils::fileExists(action_dir + "/status"));

        if (FileUtils::fileExists(action_dir + "/status")) {
            auto result = FileUtils::readFileAsString(action_dir + "/status");
            CHECK(result.find("completed") != std::string::npos);
        }

        boost::filesystem::remove_all(action_dir);
    }

    SECTION("it should correctly write the result") {
        test_reverse_module.executeDelayedAction(STRING_ACTION, msg, job_id);
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
