#include "root_path.hpp"
#include "../../common/content_format.hpp"

#include <pxp-agent/modules/apply.hpp>
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
                                          + "/lib/tests/resources/tasks-cache" };

// Disable cache ttl so we don't delete fixtures.
static const std::string CACHE_TTL { "0d" };

static const auto MODULE_CACHE_DIR = std::make_shared<ModuleCacheDir>(CACHE_DIR, CACHE_TTL);

static const std::string TEMP_CACHE_DIR { CACHE_DIR + "/temp" };

static const std::vector<std::string> MASTER_URIS { { "https://_master1", "https://_master2", "https://_master3" } };

static const std::string CA { "mock_ca" };

static const std::string CRT { "mock_crt" };

static const std::string KEY { "mock_key" };

static const std::string CRL { "mock_crl" };

static const std::string DATA_TXT {
    (DATA_FORMAT % "\"04352987\""
                 % "\"module name\""
                 % "\"action name\""
                 % "{ \"some key\" : \"some value\" }").str() };

TEST_CASE("Modules::Apply", "[modules]") {
    SECTION("can successfully instantiate") {
        REQUIRE_NOTHROW(Modules::Apply(PXP_AGENT_BIN_PATH, MASTER_URIS, CA, CRT, KEY, CRL, "", MODULE_CACHE_DIR, STORAGE));
    }
}

TEST_CASE("Modules::Apply::hasAction", "[modules]") {
    Modules::Apply mod { PXP_AGENT_BIN_PATH, MASTER_URIS, CA, CRT, KEY, CRL, "", MODULE_CACHE_DIR, STORAGE };
    SECTION("correctly reports false") {
        REQUIRE(!mod.hasAction("foo"));
    }

    SECTION("correctly reports true") {
        REQUIRE(mod.hasAction("apply"));
    }
}

// Present in Boost 1.58. That's currently not required, so reproducing it here since it's simple.
static std::time_t my_to_time_t(pt::ptime t)
{
    return (t - pt::ptime(boost::gregorian::date(1970, 1, 1))).total_seconds();
}

TEST_CASE("Modules::Apply::purge purges old cached files", "[modules]") {
    const std::string PURGE_CACHE { std::string { PXP_AGENT_ROOT_PATH }
        + "/lib/tests/resources/purge_test" };

    const std::string OLD_TRANSACTION { "valid_old" };
    const std::string RECENT_TRANSACTION { "valid_recent" };

    static const auto PURGE_MODULE_CACHE_DIR = std::make_shared<ModuleCacheDir>(PURGE_CACHE, CACHE_TTL);

    // Start with 0 TTL to prevent initial cleanup
    Modules::Apply mod { PXP_AGENT_BIN_PATH, MASTER_URIS, CA, CRT, KEY, CRL, "", PURGE_MODULE_CACHE_DIR, STORAGE };

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

TEST_CASE("Modules::buildCommandObject", "[modules]") {
    SECTION("fails when CRL is empty") {
        // the actual ActionRequest used here isn't important - the goal
        // is to test that this function now throws an exception when the
        // CRL is empty
        lth_jc::JsonContainer envelope { ENVELOPE_TXT };
        lth_jc::JsonContainer data { DATA_TXT };
        std::vector<lth_jc::JsonContainer> debug {};
        const PCPClient::ParsedChunks p_c { envelope, data, debug, 0 };

        Modules::Apply mod { PXP_AGENT_BIN_PATH, MASTER_URIS, CA, CRT, KEY, "", "", MODULE_CACHE_DIR, STORAGE };

        REQUIRE_THROWS_AS(mod.buildCommandObject(ActionRequest(RequestType::Blocking, p_c)), Configuration::Error);
    }
}
