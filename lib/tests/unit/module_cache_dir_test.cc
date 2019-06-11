#include "root_path.hpp"

#include <pxp-agent/module_cache_dir.hpp>
#include <pxp-agent/module.hpp>
#include <pxp-agent/configuration.hpp>

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

static const std::string CACHE_DIR { std::string { PXP_AGENT_ROOT_PATH }
                                          + "/lib/tests/resources/cache_dir" };
// Disable cache ttl so we don't delete fixtures.
static const std::string CACHE_TTL { "0d" };

TEST_CASE("ModuleCacheDir::ModuleCacheDir", "[modules]") {
    SECTION("can successfully instantiate") {
        REQUIRE_NOTHROW(ModuleCacheDir(CACHE_DIR, CACHE_TTL));
    }

    SECTION("Properly sets public vars") {
        ModuleCacheDir mod_cd { CACHE_DIR, CACHE_TTL };
        REQUIRE(mod_cd.cache_dir_ == CACHE_DIR);
        REQUIRE(mod_cd.purge_ttl_ == CACHE_TTL);
    }
}

// Present in Boost 1.58. That's currently not required, so reproducing it here since it's simple.
static std::time_t my_to_time_t(pt::ptime t)
{
    return (t - pt::ptime(boost::gregorian::date(1970, 1, 1))).total_seconds();
}

TEST_CASE("ModuleCacheDir::createCacheDir", "[modules]") {
    fs::remove(CACHE_DIR + "/TESTSHA");
    ModuleCacheDir mod_cd { CACHE_DIR, CACHE_TTL };
    SECTION("Creates cache dir for new SHA") {
        REQUIRE_NOTHROW(mod_cd.createCacheDir("TESTSHA"));
        REQUIRE(fs::exists(CACHE_DIR + "/TESTSHA"));
    }
    SECTION("Does not fail if cache dir already exists") {
        REQUIRE_NOTHROW(mod_cd.createCacheDir("TESTSHA"));
        REQUIRE(fs::exists(CACHE_DIR + "/TESTSHA"));
    }
    // open an output stream that will collide with the attempt to create
    // a directory and cause an exception.
    boost::nowide::ofstream(CACHE_DIR + "/TESTSHA");
    SECTION("Throws Module::ProcessingError if the attempt to create the dir throws") {
        REQUIRE_THROWS_AS(mod_cd.createCacheDir("TESTSHA"), Module::ProcessingError);
    }
}

TEST_CASE("ModuleCacheDir::purgeCache", "[modules]") {
    const std::string PURGE_TASK_CACHE { std::string { PXP_AGENT_ROOT_PATH }
        + "/lib/tests/resources/purge_test" };

    const std::string OLD_TRANSACTION { "valid_old" };
    const std::string RECENT_TRANSACTION { "valid_recent" };

    // Start with 0 TTL to prevent initial cleanup
    ModuleCacheDir mod_cd { PURGE_TASK_CACHE, CACHE_TTL };

    unsigned int num_purged_results { 0 };
    auto purgeCallback =
        [&num_purged_results](const std::string&) -> void { num_purged_results++; };

    SECTION("Purges only the old results based on ttl") {
        num_purged_results = 0;
        auto now = pt::second_clock::universal_time();
        auto recent = now - pt::minutes(50);
        auto old = now - pt::minutes(61);
        fs::last_write_time(fs::path(PURGE_TASK_CACHE)/OLD_TRANSACTION, my_to_time_t(old));
        fs::last_write_time(fs::path(PURGE_TASK_CACHE)/RECENT_TRANSACTION, my_to_time_t(recent));

        auto results = mod_cd.purgeCache("1h", {}, purgeCallback);
        REQUIRE(results == 1);
        REQUIRE(num_purged_results == 1);
    }

    auto failedCallback =
        [&num_purged_results](const std::string&) -> void { throw std::runtime_error("error"); };

    SECTION("Does not throw when purge function throws") {
        num_purged_results = 0;
        auto now = pt::second_clock::universal_time();
        auto recent = now - pt::minutes(50);
        auto old = now - pt::minutes(61);
        fs::last_write_time(fs::path(PURGE_TASK_CACHE)/OLD_TRANSACTION, my_to_time_t(old));
        fs::last_write_time(fs::path(PURGE_TASK_CACHE)/RECENT_TRANSACTION, my_to_time_t(recent));

        REQUIRE_NOTHROW(mod_cd.purgeCache("1h", {}, failedCallback));
    }
}
