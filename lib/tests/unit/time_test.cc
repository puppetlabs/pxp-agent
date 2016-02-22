#include <pxp-agent/time.hpp>

#include <leatherman/util/time.hpp>

#include <boost/date_time/posix_time/posix_time.hpp>

#include <catch.hpp>

namespace PXPAgent {

namespace pt = boost::posix_time;
namespace lth_util = leatherman::util;

TEST_CASE("Timestamp::Timestamp", "[utils][time]") {
    SECTION("can instantiate with a valid duration string") {
        REQUIRE_NOTHROW(Timestamp("15d"));
    }

    SECTION("throws an Error in case of invalid suffix") {
        REQUIRE_THROWS_AS(Timestamp("15f"),
                          Timestamp::Error);
    }

    SECTION("throws an Error in case of invalid value") {
        REQUIRE_THROWS_AS(Timestamp("abc15d"),
                          Timestamp::Error);
    }

    SECTION("the time point is actually set in the past") {
        Timestamp ts { "1m" };
        auto now = pt::microsec_clock::universal_time();
        REQUIRE(now > ts.time_point);
    }
}

TEST_CASE("Timestamp::getPastInstant", "[utils][time]") {
    SECTION("throws an Error in case of invalid duration string") {
        REQUIRE_THROWS_AS(Timestamp::getPastInstant("15f"),
                          Timestamp::Error);
    }

    SECTION("the time point is set consistently") {
        auto tp1 =  Timestamp::getPastInstant("1m");
        auto tp2 =  Timestamp::getPastInstant("1h");
        auto tp3 =  Timestamp::getPastInstant("1d");

        REQUIRE(tp1 > tp2);
        REQUIRE(tp2 > tp3);
    }
}

TEST_CASE("Timestamp::getMinutes", "[utils][time]") {
    SECTION("throws an Error in case of invalid duration string") {
        REQUIRE_THROWS_AS(Timestamp::getMinutes("15k"),
                          Timestamp::Error);
    }

    SECTION("returns the correct value") {
        SECTION("when the value is zero") {
            SECTION("when the suffix indicates days") {
                REQUIRE(Timestamp::getMinutes("0d") == 0);
            }

            SECTION("when the suffix indicates hours") {
                REQUIRE(Timestamp::getMinutes("0h") == 0);
            }

            SECTION("when the suffix indicates minutes") {
                REQUIRE(Timestamp::getMinutes("0m") == 0);
            }
        }

        SECTION("when the value is non-zero") {
            SECTION("when the suffix indicates days") {
                REQUIRE(Timestamp::getMinutes("2d") == (2 * 24 * 60));
            }

            SECTION("when the suffix indicates hours") {
                REQUIRE(Timestamp::getMinutes("100h") == (100 * 60));
            }

            SECTION("when the suffix indicates minutes") {
                REQUIRE(Timestamp::getMinutes("16m") == 16);
            }
        }
    }
}

TEST_CASE("Timestamp::convertToISO", "[utils][time]") {
    SECTION("throws an Error in case the datetime string does not end with a 'Z'") {
        REQUIRE_THROWS_AS(Timestamp::convertToISO("2016-02-18T19:40:49.711227"),
                          Timestamp::Error);
    }

    SECTION("throws an Error in case the datetime string has less than 21 chars") {
        REQUIRE_THROWS_AS(Timestamp::convertToISO("2016-02-18T19:40:49Z"),
                          Timestamp::Error);
    }

    SECTION("successfully convert the datetime format") {
        auto dt = Timestamp::convertToISO("2016-02-18T19:40:49.711227Z");
        REQUIRE(dt == "20160218T194049.711227");
    }
}

TEST_CASE("Timestamp::isNewerThan", "[utils][time]") {
    Timestamp ts { "1h" };

    SECTION("returns false if time_point is older than the datetime string arg") {
        auto newer_datetime = lth_util::get_ISO8601_time(60);
        REQUIRE_FALSE(ts.isNewerThan(newer_datetime));
    }

    SECTION("returns true if time_point is newer than the datetime string arg") {
        auto now = pt::microsec_clock::universal_time();
        auto older_time_point = now - pt::hours(2);
        auto older_datetime = pt::to_iso_extended_string(older_time_point) + "Z";

        REQUIRE(ts.isNewerThan(older_datetime));
    }
}

}  // namespace PXPAgent
