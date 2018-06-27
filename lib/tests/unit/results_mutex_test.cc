#include <pxp-agent/results_mutex.hpp>

#include <cpp-pcp-client/util/thread.hpp>

#include <catch.hpp>

using namespace PXPAgent;

TEST_CASE("ResultsMutex::exists", "[async]") {
    ResultsMutex::Instance().reset();

    SECTION("returns false correctly") {
        ResultsMutex::LockGuard lck { ResultsMutex::Instance().access_mtx };
        REQUIRE_FALSE(ResultsMutex::Instance().exists("spam"));
    }

    SECTION("returns true correctly") {
        ResultsMutex::Instance().add("beans");
        ResultsMutex::LockGuard lck { ResultsMutex::Instance().access_mtx };
        REQUIRE(ResultsMutex::Instance().exists("beans"));
    }
}

TEST_CASE("ResultsMutex::get", "[async]") {
    ResultsMutex::Instance().reset();

    SECTION("can lock with the returned mutex pointer") {
        ResultsMutex::Instance().add("spam");
        ResultsMutex::Mutex_Ptr mtx_ptr;
        ResultsMutex::Lock lck { ResultsMutex::Instance().access_mtx,
                                 PCPClient::Util::defer_lock };
        REQUIRE_FALSE(lck.owns_lock());
        lck.lock();
        REQUIRE(lck.owns_lock());
        mtx_ptr = ResultsMutex::Instance().get("spam");
        lck.unlock();
        REQUIRE_FALSE(lck.owns_lock());
        REQUIRE_NOTHROW(ResultsMutex::LockGuard(*mtx_ptr));
    }

    SECTION("throws an error if the named mutex doesn't exist") {
        ResultsMutex::LockGuard lck { ResultsMutex::Instance().access_mtx };
        REQUIRE_FALSE(ResultsMutex::Instance().exists("eggs"));
        REQUIRE_THROWS_AS(ResultsMutex::Instance().get("eggs"),
                          ResultsMutex::Error);
    }
}

TEST_CASE("ResultsMutex::add", "[async]") {
    ResultsMutex::Instance().reset();

    SECTION("can add") {
        REQUIRE_NOTHROW(ResultsMutex::Instance().add("foo"));
        REQUIRE(ResultsMutex::Instance().exists("foo"));
    }

    SECTION("throws an error when adding duplicates") {
        ResultsMutex::Instance().add("bar");
        REQUIRE_THROWS_AS(ResultsMutex::Instance().add("bar"),
                          ResultsMutex::Error);
    }
}

TEST_CASE("ResultsMutex::remove", "[async]") {
    ResultsMutex::Instance().reset();

    SECTION("can remove") {
        ResultsMutex::Instance().add("spam");
        REQUIRE(ResultsMutex::Instance().exists("spam"));
        REQUIRE_NOTHROW(ResultsMutex::Instance().remove("spam"));
        REQUIRE_FALSE(ResultsMutex::Instance().exists("spam"));

        REQUIRE_NOTHROW(ResultsMutex::Instance().add("foo"));
    }

    SECTION("throws an error if the named mutex doesn't exist") {
        REQUIRE_FALSE(ResultsMutex::Instance().exists("eggs"));
        REQUIRE_THROWS_AS(ResultsMutex::Instance().remove("eggs"),
                          ResultsMutex::Error);
    }
}
