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

static const std::string TEST_NEW_DIR { std::string { PXP_AGENT_ROOT_PATH }
                                          + "/lib/tests/resources/download_files/testing_dir" };

// Disable cache ttl so we don't delete fixtures.
static const std::string CACHE_TTL { "0d" };

static const auto MODULE_CACHE_DIR = std::make_shared<ModuleCacheDir>(CACHE_DIR, CACHE_TTL);

static const std::string TEMP_CACHE_DIR { CACHE_DIR + "/temp" };

static const std::vector<std::string> MASTER_URIS { { "https://_master1", "https://_master2", "https://_master3" } };

static const std::string CA { "mock_ca" };

static const std::string CRT { "mock_crt" };

static const std::string KEY { "mock_key" };

// Success for files is produced by calling download_file for a file that already exists.
// This way during the test no actual downloads will be attempted, and downloadFileFromMaster
// will return success without needing to download anything. Creating directories doesn't
// require any downloads, so that entry does not need to already exist.
static auto success_params = boost::format("{\"files\": ["
                                                                "{"
                                                                    "\"uri\":{"
                                                                            "\"path\":\"/dl_files/file.txt\","
                                                                            "\"params\":{"
                                                                                "\"environment\":\"production\""
                                                                            "}"
                                                                    "},"
                                                                    "\"sha256\":\"94CBA5396781C06EFB4237730751532CBEFEA4C637D17A61B2E78598F08732C2\","
                                                                    "\"destination\":\"%1%/file.txt\","
                                                                    "\"link_source\":\"\","
                                                                    "\"kind\":\"file\""
                                                                "},"
                                                                "{"
                                                                    "\"uri\":{"
                                                                            "\"path\":\"\","
                                                                            "\"params\":{}"
                                                                    "},"
                                                                    "\"sha256\":\"\","
                                                                    "\"destination\":\"%2%\","
                                                                    "\"link_source\":\"\","
                                                                    "\"kind\":\"directory\""
                                                                "}"
                                                            "]"
                                                "}") % TEST_FILE_DIR % TEST_NEW_DIR;

// Create a failure scenario by using a request with a "directory" content type where
// the destination already exists as a file
static auto dir_already_exists_params = boost::format("{\"files\": ["
                                                                "{"
                                                                    "\"uri\":{"
                                                                            "\"path\":\"\","
                                                                            "\"params\":{}"
                                                                    "},"
                                                                    "\"sha256\":\"\","
                                                                    "\"destination\":\"%1%/file.txt\","
                                                                    "\"link_source\":\"\","
                                                                    "\"kind\":\"directory\""
                                                                "}"
                                                            "]"
                                                       "}") % TEST_FILE_DIR;

// Create a failure scenario by using a request with a "directory" content type where
// the destination already exists as a file
static auto symlink_already_exists_params = boost::format("{\"files\": ["
                                                                          "{"
                                                                              "\"uri\":{"
                                                                                      "\"path\":\"\","
                                                                                      "\"params\":{}"
                                                                              "},"
                                                                              "\"sha256\":\"\","
                                                                              "\"filename\":\"\","
                                                                              "\"destination\":\"%1%/file.txt\","
                                                                              "\"link_source\":\"\","
                                                                              "\"kind\":\"symlink\""
                                                                          "}"
                                                                      "]"
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
                                                "\"destination\":\"%1%/does_not_exist.txt\","
                                                "\"link_source\":\"\","
                                                "\"kind\":\"file\""
                                             "}]"
                                    "}") % TEST_FILE_DIR;

static const PCPClient::ParsedChunks SUCCESS_NON_BLOCKING_CONTENT {
                    lth_jc::JsonContainer(ENVELOPE_TXT),           // envelope
                    lth_jc::JsonContainer(std::string { (NON_BLOCKING_DATA_FORMAT % "\"1988\""
                                                                                  % "\"download_file\""
                                                                                  % "\"download\""
                                                                                  % success_params
                                                                                  % "false").str() }),  // data
                    {},   // debug
                    0 };  // num invalid debug chunks

static const PCPClient::ParsedChunks FAILURE_DIR_EXISTS_AS_FILE_CONTENT {
                    lth_jc::JsonContainer(ENVELOPE_TXT),           // envelope
                    lth_jc::JsonContainer(std::string { (NON_BLOCKING_DATA_FORMAT % "\"1988\""
                                                                                  % "\"downloadfile\""
                                                                                  % "\"download\""
                                                                                  % dir_already_exists_params
                                                                                  % "false").str() }),  // data
                    {},   // debug
                    0 };  // num invalid debug chunks

static const PCPClient::ParsedChunks FAILURE_SYMLINK_EXISTS_AS_FILE_CONTENT {
                    lth_jc::JsonContainer(ENVELOPE_TXT),           // envelope
                    lth_jc::JsonContainer(std::string { (NON_BLOCKING_DATA_FORMAT % "\"1988\""
                                                                                  % "\"downloadfile\""
                                                                                  % "\"download\""
                                                                                  % symlink_already_exists_params
                                                                                  % "false").str() }),  // data
                    {},   // debug
                    0 };  // num invalid debug chunks

static const PCPClient::ParsedChunks FAILURE_NON_BLOCKING_CONTENT {
                    lth_jc::JsonContainer(ENVELOPE_TXT),           // envelope
                    lth_jc::JsonContainer(std::string { (NON_BLOCKING_DATA_FORMAT % "\"1988\""
                                                                                  % "\"download_file\""
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
    SECTION("Correctly returns exitcode 0 when call succeeds") {
        // Remove the directory to be created in case it was here from a previous test
        fs::remove(fs::path(TEST_NEW_DIR));
        ActionRequest request { RequestType::NonBlocking, SUCCESS_NON_BLOCKING_CONTENT };
        auto response = mod.executeAction(request);
        REQUIRE(response.output.exitcode == 0);
        SECTION("Correctly created new directory") {
            REQUIRE(fs::exists(fs::path(TEST_NEW_DIR)));
            // Remove the newly created directory after testing
            fs::remove(fs::path(TEST_NEW_DIR));
        }
    }

    SECTION("Correctly fails if the new directory requested is already a file") {
        // FAILURE_DIR_EXISTS_AS_FILE_CONTENT will fail because it will make a request to make a
        // directory in a location where there's already a file of that name.
        ActionRequest request { RequestType::NonBlocking, FAILURE_DIR_EXISTS_AS_FILE_CONTENT };
        auto response = mod.executeAction(request);
        REQUIRE_FALSE(response.action_metadata.includes("results"));
        REQUIRE_FALSE(response.action_metadata.get<bool>("results_are_valid"));
        REQUIRE(response.action_metadata.includes("execution_error"));
        // check that the file did not change to a directory
        REQUIRE(fs::is_regular_file(fs::path(TEST_FILE_DIR + "/file.txt")));
    }

    SECTION("Correctly fails if the new symlink requested is already a file") {
        // FAILURE_SYMLINK_EXISTS_AS_FILE_CONTENT will fail because it will make a request to make a
        // symlink in a location where there's already a file of that name.
        ActionRequest request { RequestType::NonBlocking, FAILURE_SYMLINK_EXISTS_AS_FILE_CONTENT };
        auto response = mod.executeAction(request);
        REQUIRE_FALSE(response.action_metadata.includes("results"));
        REQUIRE_FALSE(response.action_metadata.get<bool>("results_are_valid"));
        REQUIRE(response.action_metadata.includes("execution_error"));
        // check that the file did not change to a directory
        REQUIRE(fs::is_regular_file(fs::path(TEST_FILE_DIR + "/file.txt")));
    }

    SECTION("Correctly fails with a failed download") {
        // FAILURE_NON_BLOCKING_CONTENT will fail because it will actually force an attempted download
        // from the master URIs, which aren't real.
        ActionRequest request { RequestType::NonBlocking, FAILURE_NON_BLOCKING_CONTENT };
        auto response = mod.executeAction(request);
        REQUIRE_FALSE(response.action_metadata.includes("results"));
        REQUIRE_FALSE(response.action_metadata.get<bool>("results_are_valid"));
        REQUIRE(response.action_metadata.includes("execution_error"));
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
