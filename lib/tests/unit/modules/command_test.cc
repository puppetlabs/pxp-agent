#include <catch.hpp>
#include "root_path.hpp"
#include "../../common/content_format.hpp"

#include <boost/algorithm/string.hpp>

#include <leatherman/execution/execution.hpp>
#include <leatherman/util/scope_exit.hpp>
#include <leatherman/file_util/file.hpp>

#include <pxp-agent/configuration.hpp>
#include <pxp-agent/modules/command.hpp>

using namespace PXPAgent;

namespace fs = boost::filesystem;
namespace lth_exe = leatherman::execution;
namespace lth_file = leatherman::file_util;
namespace lth_jc = leatherman::json_container;

static const std::string SPOOL_DIR { std::string { PXP_AGENT_ROOT_PATH }
                                     + "/lib/tests/resources/test_spool" };
static const auto STORAGE = std::make_shared<ResultsStorage>(SPOOL_DIR, "0d");

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

static ActionRequest command_request(const std::string& params_txt, std::string transaction_id = "0987") {
    std::string command_txt {
            (NON_BLOCKING_DATA_FORMAT % boost::io::quoted(transaction_id)
            % "\"command\""
            % "\"run\""
            % params_txt
            % "false").str()
    };

    PCPClient::ParsedChunks command_content {
        lth_jc::JsonContainer(ENVELOPE_TXT),
        lth_jc::JsonContainer(command_txt),
        {},
        0
    };

    ActionRequest request { RequestType::NonBlocking, command_content };
    fs::path spool_path { SPOOL_DIR };
    auto results_dir = (spool_path / request.transactionId()).string();
    fs::create_directories(results_dir);
    request.setResultsDir(results_dir);

    return request;
}

TEST_CASE("Modules::Command", "[modules]") {
    SECTION("can successfully instantiate") {
        REQUIRE_NOTHROW(Modules::Command(PXP_AGENT_BIN_PATH, STORAGE));
    }
}

TEST_CASE("Modules::Command::hasAction", "[modules]") {
    Modules::Command mod { PXP_AGENT_BIN_PATH, STORAGE };

    SECTION("correctly reports false for a nonexistent action") {
        REQUIRE_FALSE(mod.hasAction("foo"));
    }

    SECTION("correctly reports true for a real action") {
        REQUIRE(mod.hasAction("run"));
    }
}

TEST_CASE("Modules::Command::callAction successfully", "[modules]") {
    configureTest();
    lth_util::scope_exit config_cleaner { resetTest };
    Modules::Command mod { PXP_AGENT_BIN_PATH, STORAGE };

#ifdef _WIN32
    static const std::string echo_params { "{ \"command\": \"cmd.exe /c echo hello\" }" };
#else
    static const std::string echo_params { "{ \"command\": \"echo hello\" }" };
#endif

    auto request = command_request(echo_params);
    auto response = mod.executeAction(request);

    SECTION("stdout is written to a file") {
        boost::trim(response.output.std_out);
        REQUIRE(response.output.std_out == "hello");
    }

    SECTION("stderr is written to a file") {
        REQUIRE(response.output.std_err == "");
    }

    SECTION("exit code is written to a file") {
        REQUIRE(response.output.exitcode == 0);
    }

    SECTION("PID is written to a file") {
        fs::path spool_path { SPOOL_DIR };
        auto pid_path = spool_path / request.transactionId() / "pid";
        REQUIRE(fs::exists(pid_path));

        try {
            auto pid_txt = lth_file::read(pid_path.string());
            auto pid = std::stoi(pid_txt);
        } catch (std::exception&) {
            FAIL("Failed to read PID");
        }
    }
}

TEST_CASE("Modules::Command::callAction where the command is not found", "[modules]") {
    configureTest();
    lth_util::scope_exit config_cleaner { resetTest };
    Modules::Command mod { PXP_AGENT_BIN_PATH, STORAGE };

    static const std::string missing_command { "{ \"command\": \"not-a-real-command\" }" };
    auto request = command_request(missing_command);
    auto response = mod.executeAction(request);

    SECTION("the execution result itself is still valid") {
        REQUIRE(response.action_metadata.get<bool>("results_are_valid"));
    }

    SECTION("stdout is written to a file and is empty") {
        REQUIRE(response.output.std_out == "");
    }

    // NOTE: the exit code is the only information we get from leatherman::execution when the
    // command is not found, so stderr is empty here. pxp-agent should be logging a message
    // indicating that the command wasn't found when the exit code is 127 and both stdout and
    // stderr are empty (see BoltModule's output processing).

    SECTION("stderr is written to a file and is empty") {
        REQUIRE(response.output.std_err == "");
    }

    SECTION("exit code is written to a file and read into the result object") {
        REQUIRE(response.output.exitcode == 127);
    }

    SECTION("PID is written to a file") {
        fs::path spool_path { SPOOL_DIR };
        auto pid_path = spool_path / request.transactionId() / "pid";
        REQUIRE(fs::exists(pid_path));

        try {
            auto pid_txt = lth_file::read(pid_path.string());
            auto pid = std::stoi(pid_txt);
        } catch (std::exception&) {
            FAIL("Failed to read PID");
        }
    }
}

TEST_CASE("Modules::Command::callAction where the command fails", "[modules]") {
    configureTest();
    lth_util::scope_exit config_cleaner { resetTest };
    Modules::Command mod { PXP_AGENT_BIN_PATH, STORAGE };

#ifdef _WIN32
    static const std::string bad_command { "{ \"command\": \"powershell.exe -not-a-real-option\" }" };
#else
    static const std::string bad_command { "{ \"command\": \"ls -not-a-real-option\" }" };
#endif
    auto request = command_request(bad_command);
    auto response = mod.executeAction(request);

    SECTION("the execution result itself is still valid") {
        REQUIRE(response.action_metadata.get<bool>("results_are_valid"));
    }

    SECTION("stdout is written to a file and is empty") {
        REQUIRE(response.output.std_out == "");
    }

    SECTION("stderr is written to a file and contains a message") {
        REQUIRE_FALSE(response.output.std_err == "");
    }

    SECTION("exit code is written to a file and indicates failure") {
        REQUIRE(response.output.exitcode > 0);
        REQUIRE(response.output.exitcode != 127);  // This would be the "not found" exit code
    }

    SECTION("PID is written to a file") {
        fs::path spool_path { SPOOL_DIR };
        auto pid_path = spool_path / request.transactionId() / "pid";
        REQUIRE(fs::exists(pid_path));

        try {
            auto pid_txt = lth_file::read(pid_path.string());
            auto pid = std::stoi(pid_txt);
        } catch (std::exception&) {
            FAIL("Failed to read PID");
        }
    }
}

// TEST_CASE("Modules::Command::buildCommandObject where the executable contains spaces", "[modules]") {
//     Modules::Command mod { PXP_AGENT_BIN_PATH, STORAGE };

//     SECTION("double-quoted executable is separated from the arguments") {
//         auto request = command_request("{ \"command\": \"\\\"C:\\\\Program Files (x86)\\\\Executable with Spaces.exe\\\" <arg1> <arg2>\"}");
//         auto cmd = mod.buildCommandObject(request);
//         REQUIRE(cmd.executable == "C:\\Program Files (x86)\\Executable with Spaces.exe");
//         REQUIRE(cmd.arguments.at(0) == "<arg1>");
//         REQUIRE(cmd.arguments.at(1) == "<arg2>");
//     }

//     SECTION("single-quoted executable is separated from the arguments") {
//         auto request = command_request("{ \"command\": \"'C:\\\\Program Files (x86)\\\\Executable with Spaces.exe' <arg1> <arg2>\"}");
//         auto cmd = mod.buildCommandObject(request);
//         REQUIRE(cmd.executable == "C:\\Program Files (x86)\\Executable with Spaces.exe");
//         REQUIRE(cmd.arguments.at(0) == "<arg1>");
//         REQUIRE(cmd.arguments.at(1) == "<arg2>");
//     }
// }

// TEST_CASE("Module::Command::buildCommandObject where the arguments contain spaces", "[modules]") {
//     Modules::Command mod { PXP_AGENT_BIN_PATH, STORAGE };

//     SECTION("double-quoted arguments are correctly parsed") {
//         auto request = command_request("{ \"command\": \"rm -f \\\"Untitled 1.rtf\\\" \\\"Untitled 2.rtf\\\"\" }");
//         auto cmd = mod.buildCommandObject(request);
//         REQUIRE(cmd.executable == "rm");
//         REQUIRE(cmd.arguments.at(0) == "-f");
//         REQUIRE(cmd.arguments.at(1) == "Untitled 1.rtf");
//         REQUIRE(cmd.arguments.at(2) == "Untitled 2.rtf");
//     }

//     SECTION("single-quoted arguments are correctly parsed") {
//         auto request = command_request("{ \"command\": \"rm -f 'Untitled 1.rtf' 'Untitled 2.rtf'\" }");
//         auto cmd = mod.buildCommandObject(request);
//         REQUIRE(cmd.executable == "rm");
//         REQUIRE(cmd.arguments.at(0) == "-f");
//         REQUIRE(cmd.arguments.at(1) == "Untitled 1.rtf");
//         REQUIRE(cmd.arguments.at(2) == "Untitled 2.rtf");
//     }
// }
