#include "root_path.hpp"
#include "../../common/content_format.hpp"

#include <pxp-agent/modules/script.hpp>
#include <pxp-agent/configuration.hpp>
#include <pxp-agent/util/process.hpp>

#include <cpp-pcp-client/protocol/chunks.hpp>       // ParsedChunks

#include <leatherman/json_container/json_container.hpp>
#include <leatherman/util/scope_exit.hpp>
#include <leatherman/file_util/file.hpp>

#include <boost/filesystem.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/erase.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>

#include <catch.hpp>

#include <string>
#include <vector>
#include <unistd.h>

using namespace PXPAgent;

namespace fs = boost::filesystem;
namespace pt = boost::posix_time;
namespace lth_jc = leatherman::json_container;
namespace lth_util = leatherman::util;
namespace lth_file = leatherman::file_util;

#ifdef _WIN32
    static const std::string TESTING_SCRIPT = "test_script.ps1";
    static const std::string TESTING_SCRIPT_SHA265 = "3D57FCC5FBDC53E667D16493D38621F36B2CF7DB244986B07A4FBCFECE9E19BD";
#else
    static const std::string TESTING_SCRIPT = "test_script.sh";
    static const std::string TESTING_SCRIPT_SHA265 = "71A2725F36221AAE75AF076D01D18744DA461A02DD2AB746447BC41C71248562";
#endif

static const std::string SPOOL_DIR { std::string { PXP_AGENT_ROOT_PATH }
                                     + "/lib/tests/resources/test_spool" };

static const auto STORAGE = std::make_shared<ResultsStorage>(SPOOL_DIR, "0d");

static const std::string CACHE_DIR { std::string { PXP_AGENT_ROOT_PATH }
                                          + "/lib/tests/resources/scripts_cache" };

// Disable cache ttl so we don't delete fixtures.
static const std::string CACHE_TTL { "0d" };

static const auto MODULE_CACHE_DIR = std::make_shared<ModuleCacheDir>(CACHE_DIR, CACHE_TTL);

static const std::string TEMP_CACHE_DIR { CACHE_DIR + "/temp" };

static const std::vector<std::string> MASTER_URIS { { "https://_master1", "https://_master2", "https://_master3" } };

static const std::string CA { "mock_ca" };

static const std::string CRT { "mock_crt" };

static const std::string KEY { "mock_key" };

static const ActionRequest script_request(const std::string& destination, const std::string& sha, const std::vector<std::string>& args) {
    std::string paramsStr = (boost::format("{\"script\": {"
                                                        "\"uri\":{"
                                                                "\"path\":\"/dl_files/fake-request-file.txt\","
                                                                "\"params\":{"
                                                                    "\"environment\":\"production\""
                                                                "}"
                                                        "},"
                                                        "\"sha256\":\"%1%\","
                                                        "\"filename\":\"%2%\""
                                                    "}"
                                        "}") % sha % destination).str();
    auto paramsJson = lth_jc::JsonContainer(paramsStr);
    paramsJson.set("arguments", args);

    const PCPClient::ParsedChunks non_blocking_content {
        lth_jc::JsonContainer(ENVELOPE_TXT),           // envelope
        lth_jc::JsonContainer(std::string { (NON_BLOCKING_DATA_FORMAT % "\"1988\""
                                                                        % "\"script\""
                                                                        % "\"run\""
                                                                        % paramsJson.toString()
                                                                        % "false").str() }),  // data
        {},   // debug
        0 };
    ActionRequest request { RequestType::NonBlocking, non_blocking_content };
    fs::path spool_path { SPOOL_DIR };
    auto results_dir = (spool_path / request.transactionId()).string();
    fs::create_directories(results_dir);
    request.setResultsDir(results_dir);

    return request;
}

TEST_CASE("Modules::Script", "[modules]") {
    SECTION("can successfully instantiate") {
        REQUIRE_NOTHROW(Modules::Script(PXP_AGENT_BIN_PATH, MASTER_URIS, CA, CRT, KEY, "", 10, 20, MODULE_CACHE_DIR, STORAGE));
    }
}

TEST_CASE("Modules::Script::hasAction", "[modules]") {
    Modules::Script mod { PXP_AGENT_BIN_PATH, MASTER_URIS, CA, CRT, KEY, "", 10, 20, MODULE_CACHE_DIR, STORAGE };
    SECTION("correctly reports false") {
        REQUIRE(!mod.hasAction("foo"));
    }

    SECTION("correctly reports true") {
        REQUIRE(mod.hasAction("run"));
    }
}

TEST_CASE("Modules::Script can execute a script", "[modules]") {
    Modules::Script mod { PXP_AGENT_BIN_PATH, MASTER_URIS, CA, CRT, KEY, "", 10, 20, MODULE_CACHE_DIR, STORAGE };
    SECTION("script executes and writes to stdout") {
        auto args = std::vector<std::string>();
        auto response = mod.executeAction(script_request(TESTING_SCRIPT, TESTING_SCRIPT_SHA265, args));
        boost::trim(response.output.std_out);
        REQUIRE(response.output.std_out == "TESTING");
        REQUIRE(response.output.std_err == "");
        REQUIRE(response.output.exitcode == 0);
    }
    SECTION("script executes with multiple parameters") {
        auto args = std::vector<std::string>({"THREE", "TEST", "ARGS"});
        auto response = mod.executeAction(script_request(TESTING_SCRIPT, TESTING_SCRIPT_SHA265, args));
        boost::trim(response.output.std_out);
        REQUIRE(response.output.std_out == "| THREE | TEST | ARGS");
        REQUIRE(response.output.std_err == "");
        REQUIRE(response.output.exitcode == 0);
    }
#ifdef _WIN32
    SECTION("script executes with a filename that has spaces") {
        auto args = std::vector<std::string>();
        auto response = mod.executeAction(script_request("test script.ps1", TESTING_SCRIPT_SHA265, args));
        boost::trim(response.output.std_out);
        REQUIRE(response.output.std_out == "TESTING");
        REQUIRE(response.output.std_err == "");
        REQUIRE(response.output.exitcode == 0);
    }
#endif
}

TEST_CASE("Modules::Script correctly reports failures", "[modules]") {
    Modules::Script mod { PXP_AGENT_BIN_PATH, MASTER_URIS, CA, CRT, KEY, "", 10, 20, MODULE_CACHE_DIR, STORAGE };
    SECTION("Reports exit code 1 when the script fails") {
        auto args = std::vector<std::string>({"FAIL"});
        auto response = mod.executeAction(script_request(TESTING_SCRIPT, TESTING_SCRIPT_SHA265, args));
        boost::trim(response.output.std_err);
        REQUIRE(response.output.std_out == "");
        REQUIRE(response.output.std_err.find("FAIL TESTING") != std::string::npos);
        REQUIRE(response.output.exitcode == 5);
    }

    // This will produce a failure because the file does not exist yet, so the module
    // will attempt the download from master URIs that aren't real.
    SECTION("Reports invalid results on failed file download") {
        auto args = std::vector<std::string>({"FAIL"});
        auto response = mod.executeAction(script_request("script_failure.rb", "SCRIPT_FAILING_SHA", args));
        REQUIRE_FALSE(response.action_metadata.get<bool>("results_are_valid"));
        REQUIRE(response.action_metadata.includes("execution_error"));
    }
}

// Present in Boost 1.58. That's currently not required, so reproducing it here since it's simple.
static std::time_t my_to_time_t(pt::ptime t)
{
    return (t - pt::ptime(boost::gregorian::date(1970, 1, 1))).total_seconds();
}

TEST_CASE("Modules::Script::purge purges old cached files", "[modules]") {
    const std::string PURGE_CACHE { std::string { PXP_AGENT_ROOT_PATH }
        + "/lib/tests/resources/purge_test" };

    const std::string OLD_TRANSACTION { "valid_old" };
    const std::string RECENT_TRANSACTION { "valid_recent" };

    static const auto PURGE_MODULE_CACHE_DIR = std::make_shared<ModuleCacheDir>(PURGE_CACHE, CACHE_TTL);

    // Start with 0 TTL to prevent initial cleanup
    Modules::Script mod { PXP_AGENT_BIN_PATH, MASTER_URIS, CA, CRT, KEY, "", 10, 20, PURGE_MODULE_CACHE_DIR, STORAGE };

    unsigned int num_purged_results { 0 };
    auto purgeCallback =
        [&num_purged_results](const std::string&) -> void { num_purged_results++; };

    SECTION("Purges only the old results based on ttl") {
        num_purged_results = 0;
        auto now = pt::second_clock::universal_time();
        auto recent = now - pt::minutes(50);
        auto old = now - pt::minutes(61);
        fs::last_write_time(fs::path(PURGE_CACHE)/OLD_TRANSACTION, my_to_time_t(old));
        fs::last_write_time(fs::path(PURGE_CACHE)/RECENT_TRANSACTION, my_to_time_t(recent));

        auto results = mod.purge("1h", {}, purgeCallback);
        REQUIRE(results == 1);
        REQUIRE(num_purged_results == 1);
    }
}
