#include "root_path.hpp"
#include "../../common/content_format.hpp"

#include <pxp-agent/modules/task.hpp>
#include <pxp-agent/configuration.hpp>
#include <pxp-agent/util/process.hpp>

#include <cpp-pcp-client/protocol/chunks.hpp>       // ParsedChunks

#include <leatherman/json_container/json_container.hpp>
#include <leatherman/util/scope_exit.hpp>
#include <leatherman/file_util/file.hpp>

#include <boost/filesystem/operations.hpp>
#include <boost/algorithm/string/trim.hpp>

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
namespace lth_jc = leatherman::json_container;
namespace lth_util = leatherman::util;
namespace lth_file = leatherman::file_util;

static const std::string SPOOL_DIR { std::string { PXP_AGENT_ROOT_PATH }
                                     + "/lib/tests/resources/test_spool" };

static const std::string TASK_CACHE_DIR { std::string { PXP_AGENT_ROOT_PATH }
                                          + "/lib/tests/resources/tasks" };

static const std::string NON_BLOCKING_ECHO_TXT {
    (NON_BLOCKING_DATA_FORMAT % "\"1988\""
                              % "\"task\""
                              % "\"run\""
                              % "{\"task\":\"foo\",\"input\":{\"message\":\"hello\"},"
                                 "\"files\":[{\"uri\":{\"path\":\"/init" EXTENSION "\","
                                                      "\"params\":{\"environment\":\"production\"}},"
                                                      "\"sha256\":\"d5296d3bd81d1245646f3467531349120519eb771b8e7a919b5fe3d3036537cb\","
                                                      "\"filename\":\"init\","
                                                      "\"size_bytes\":131}]}"
                              % "false").str() };

static const PCPClient::ParsedChunks NON_BLOCKING_CONTENT {
                    lth_jc::JsonContainer(ENVELOPE_TXT),           // envelope
                    lth_jc::JsonContainer(NON_BLOCKING_ECHO_TXT),  // data
                    {},   // debug
                    0 };  // num invalid debug chunks

TEST_CASE("Modules::Task", "[modules]") {
    SECTION("can successfully instantiate") {
        REQUIRE_NOTHROW(Modules::Task(PXP_AGENT_BIN_PATH, TASK_CACHE_DIR, SPOOL_DIR));
    }
}

TEST_CASE("Modules::Task::hasAction", "[modules]") {
    Modules::Task mod { PXP_AGENT_BIN_PATH, TASK_CACHE_DIR, SPOOL_DIR };

    SECTION("correctly reports false") {
        REQUIRE(!mod.hasAction("foo"));
    }

    SECTION("correctly reports true") {
        REQUIRE(mod.hasAction("run"));
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

TEST_CASE("Modules::Task::callAction - non blocking", "[modules]") {
    configureTest();
    lth_util::scope_exit config_cleaner { resetTest };

    SECTION("the pid is written to file") {
        Modules::Task e_m { PXP_AGENT_BIN_PATH, TASK_CACHE_DIR, SPOOL_DIR };
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

TEST_CASE("Modules::Task::executeAction", "[modules][output]") {
    configureTest();
    lth_util::scope_exit config_cleaner { resetTest };

    SECTION("passes input as json") {
        Modules::Task e_m { PXP_AGENT_BIN_PATH, TASK_CACHE_DIR, SPOOL_DIR };
        auto echo_txt = (DATA_FORMAT % "\"0632\""
                                     % "\"task\""
                                     % "\"run\""
                                     % "{\"input\":{\"message\":\"hello\"}, \"files\" : [{\"uri\": {\"path\": \"/init" EXTENSION "\"}}]}").str();
        PCPClient::ParsedChunks echo_content {
            lth_jc::JsonContainer(ENVELOPE_TXT),
            lth_jc::JsonContainer(echo_txt),
            {},
            0 };
        ActionRequest request { RequestType::Blocking, echo_content };

        auto output = e_m.executeAction(request).action_metadata.get<std::string>({ "results", "output" });
        boost::trim(output);
        REQUIRE(output == "{\"message\":\"hello\"}");
    }

    SECTION("passes input as env variables") {
        Modules::Task e_m { PXP_AGENT_BIN_PATH, TASK_CACHE_DIR, SPOOL_DIR };
        auto echo_txt = (DATA_FORMAT % "\"0632\""
                                     % "\"task\""
                                     % "\"run\""
                                     % "{\"input\":{\"message\":\"hello\"}, \"files\" : [{\"uri\": {\"path\": \"/printer" EXTENSION "\"}}]}").str();
        PCPClient::ParsedChunks echo_content {
            lth_jc::JsonContainer(ENVELOPE_TXT),
            lth_jc::JsonContainer(echo_txt),
            {},
            0 };
        ActionRequest request { RequestType::Blocking, echo_content };

        auto output = e_m.executeAction(request).action_metadata.get<std::string>({ "results", "output" });
        boost::trim(output);
        REQUIRE(output == "hello");
    }

    SECTION("succeeds on non-zero exit") {
        Modules::Task e_m { PXP_AGENT_BIN_PATH, TASK_CACHE_DIR, SPOOL_DIR };
        auto echo_txt = (DATA_FORMAT % "\"0632\""
                                     % "\"task\""
                                     % "\"run\""
                                     % "{\"input\":{\"message\":\"hello\"}, \"files\" : [{\"uri\": {\"path\": \"/error" EXTENSION "\"}}]}").str();
        PCPClient::ParsedChunks echo_content {
            lth_jc::JsonContainer(ENVELOPE_TXT),
            lth_jc::JsonContainer(echo_txt),
            {},
            0 };
        ActionRequest request { RequestType::Blocking, echo_content };
        auto response = e_m.executeAction(request);

        auto output = response.action_metadata.get<std::string>({ "results", "output" });
        boost::trim(output);
        REQUIRE(output == "hello");
        REQUIRE(response.action_metadata.get<bool>("results_are_valid"));
        boost::trim(response.output.std_out);
        REQUIRE(response.output.std_out == "hello");
        boost::trim(response.output.std_err);
        REQUIRE(response.output.std_err == "goodbye");
        REQUIRE(response.output.exitcode == 1);
    }

    SECTION("errors on unparseable output") {
        Modules::Task e_m { PXP_AGENT_BIN_PATH, TASK_CACHE_DIR, SPOOL_DIR };
        auto echo_txt = (DATA_FORMAT % "\"0632\""
                                     % "\"task\""
                                     % "\"run\""
                                     % "{\"task\": \"unparseable\", \"input\":{\"message\":\"hello\"}, "
                                       "\"files\" : [{\"uri\": {\"path\": \"/unparseable" EXTENSION "\"}}]}").str();
        PCPClient::ParsedChunks echo_content {
            lth_jc::JsonContainer(ENVELOPE_TXT),
            lth_jc::JsonContainer(echo_txt),
            {},
            0 };
        ActionRequest request { RequestType::Blocking, echo_content };
        auto response = e_m.executeAction(request);

        REQUIRE_FALSE(response.action_metadata.includes("results"));
        REQUIRE_FALSE(response.action_metadata.get<bool>("results_are_valid"));
        REQUIRE(response.action_metadata.includes("execution_error"));
        REQUIRE(response.action_metadata.get<std::string>("execution_error") == "The task executed for the blocking 'task run' request (transaction 0632) returned invalid UTF-8 on stdout - stderr: (empty)");
        boost::trim(response.output.std_out);
        unsigned char badchar[3] = {0xed, 0xbf, 0xbf};
        REQUIRE(response.output.std_out == std::string(reinterpret_cast<char*>(badchar), 3));
        REQUIRE(response.output.std_err == "");
        REQUIRE(response.output.exitcode == 0);
    }
}

}  // namespace PXPAgent
