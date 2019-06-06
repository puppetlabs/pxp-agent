#include "root_path.hpp"
#include "../../common/content_format.hpp"

#include <pxp-agent/modules/download_file.hpp>
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

static const std::string SPOOL_DIR { std::string { PXP_AGENT_ROOT_PATH }
                                     + "/lib/tests/resources/test_spool" };

static const auto STORAGE = std::make_shared<ResultsStorage>(SPOOL_DIR, "0d");

static const std::string CACHE_DIR { std::string { PXP_AGENT_ROOT_PATH }
                                          + "/lib/tests/resources/cache_dir" };
static const std::string TEST_FILE_DIR { std::string { PXP_AGENT_ROOT_PATH }
                                          + "/lib/tests/resources/download_files" };


// Disable cache ttl so we don't delete fixtures.
static const std::string CACHE_TTL { "0d" };

static const auto MODULE_CACHE_DIR = std::make_shared<ModuleCacheDir>(CACHE_DIR, CACHE_TTL);

static const std::string TEMP_CACHE_DIR { CACHE_DIR + "/temp" };

static const std::vector<std::string> MASTER_URIS { { "https://_master1", "https://_master2", "https://_master3" } };

static const std::string CA { "mock_ca" };

static const std::string CRT { "mock_crt" };

static const std::string KEY { "mock_key" };

static auto success_params = boost::format("{\"files\": [{"
                                                "\"uri\":{"
                                                        "\"path\":\"/dl_files/file.txt\","
                                                        "\"params\":{"
                                                            "\"environment\":\"production\""
                                                        "}"
                                                "},"
                                                "\"sha256\":\"94CBA5396781C06EFB4237730751532CBEFEA4C637D17A61B2E78598F08732C2\","
                                                "\"filename\":\"file.txt\","
                                                "\"destination\":\"%1%/file.txt\""
                                             "}]"
                                    "}") % TEST_FILE_DIR;

// Failures are produced by providing a file that does not exist on the filesystem yet,
// which would force a download that will fail since none of the mock master uris are
// real.
static auto failure_params = boost::format("{\"files\": [{"
                                                "\"uri\":{"
                                                        "\"path\":\"/dl_files/does_not_exist.txt\","
                                                        "\"params\":{"
                                                            "\"environment\":\"production\""
                                                        "}"
                                                "},"
                                                "\"sha256\":\"FAKESHA256\","
                                                "\"filename\":\"does_not_exist.txt\","
                                                "\"destination\":\"%1%/does_not_exist.txt\""
                                             "}]"
                                    "}") % TEST_FILE_DIR;

static const PCPClient::ParsedChunks SUCCESS_NON_BLOCKING_CONTENT {
                    lth_jc::JsonContainer(ENVELOPE_TXT),           // envelope
                    lth_jc::JsonContainer(std::string { (NON_BLOCKING_DATA_FORMAT % "\"1988\""
                                                                                  % "\"downloadfile\""
                                                                                  % "\"download\""
                                                                                  % success_params
                                                                                  % "false").str() }),  // data
                    {},   // debug
                    0 };  // num invalid debug chunks

static const PCPClient::ParsedChunks FAILURE_NON_BLOCKING_CONTENT {
                    lth_jc::JsonContainer(ENVELOPE_TXT),           // envelope
                    lth_jc::JsonContainer(std::string { (NON_BLOCKING_DATA_FORMAT % "\"1988\""
                                                                                  % "\"downloadfile\""
                                                                                  % "\"download\""
                                                                                  % failure_params
                                                                                  % "false").str() }),  // data
                    {},   // debug
                    0 };  // num invalid debug chunks

TEST_CASE("Modules::DownloadFile", "[modules]") {
    SECTION("can successfully instantiate") {
        REQUIRE_NOTHROW(Modules::DownloadFile(MASTER_URIS, CA, CRT, KEY, "", 10, 20, MODULE_CACHE_DIR, STORAGE));
    }
}

TEST_CASE("Modules::DownloadFile::hasAction", "[modules]") {
    Modules::DownloadFile mod { MASTER_URIS, CA, CRT, KEY, "", 10, 20, MODULE_CACHE_DIR, STORAGE };
    SECTION("correctly reports false") {
        REQUIRE(!mod.hasAction("foo"));
    }

    SECTION("correctly reports true") {
        REQUIRE(mod.hasAction("download"));
    }
}

TEST_CASE("Modules::DownloadFile::callAction", "[modules]") {
    Modules::DownloadFile mod { MASTER_URIS, CA, CRT, KEY, "", 10, 20, MODULE_CACHE_DIR, STORAGE };
    SECTION("Correctly returns exitcode 0 with a successful call to downloadFileFromMaster") {
        ActionRequest request { RequestType::NonBlocking, SUCCESS_NON_BLOCKING_CONTENT };
        auto response = mod.executeAction(request);
        REQUIRE(response.output.exitcode == 0);
    }

    SECTION("Correctly returns exitcode 1 with a failed download") {
        // FAILURE_NON_BLOCKING_CONTENT will fail because it will actually force an attempted download
        // from the master URIs, which aren't real.
        ActionRequest request { RequestType::NonBlocking, FAILURE_NON_BLOCKING_CONTENT };
        auto response = mod.executeAction(request);
        REQUIRE(response.output.exitcode == 1);
    }
}

// Present in Boost 1.58. That's currently not required, so reproducing it here since it's simple.
static std::time_t my_to_time_t(pt::ptime t)
{
    return (t - pt::ptime(boost::gregorian::date(1970, 1, 1))).total_seconds();
}

TEST_CASE("Modules::DownloadFile::purge purges old downloaded files", "[modules]") {
    const std::string PURGE_CACHE { std::string { PXP_AGENT_ROOT_PATH }
        + "/lib/tests/resources/purge_test" };

    const std::string OLD_TRANSACTION { "valid_old" };
    const std::string RECENT_TRANSACTION { "valid_recent" };

    static const auto PURGE_MODULE_CACHE_DIR = std::make_shared<ModuleCacheDir>(PURGE_CACHE, CACHE_TTL);

    // Start with 0 TTL to prevent initial cleanup
    Modules::DownloadFile mod { MASTER_URIS, CA, CRT, KEY, "", 10, 20, PURGE_MODULE_CACHE_DIR, STORAGE };

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