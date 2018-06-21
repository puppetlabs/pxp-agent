#include <pxp-agent/util/process.hpp>

#include <catch.hpp>

#ifdef _WIN32
    #include <leatherman/windows/windows.hpp>
    #undef ERROR
#else
    #include <unistd.h>
#endif

using namespace PXPAgent;
using namespace Util;

TEST_CASE("processExists", "[util]") {
    SECTION("this process is executing") {
#ifdef _WIN32
        REQUIRE(processExists(GetCurrentProcessId()));
#else
        REQUIRE(processExists(getpid()));
#endif
    }
}

TEST_CASE("getPid", "[util]") {
    SECTION("can call it") {
        REQUIRE_NOTHROW(getPid());
    }
}
