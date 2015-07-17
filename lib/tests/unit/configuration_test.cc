#include "certs.hpp"

#include "root_path.hpp"

#include <cthun-agent/configuration.hpp>
#include <cthun-agent/errors.hpp>

#include "horsewhisperer/horsewhisperer.h"

#include <catch.hpp>

#include <string>

namespace CthunAgent {

namespace HW = HorseWhisperer;

const std::string SERVER = "wss:///test_server";
const std::string CA = getCaPath();
const std::string CERT = getCertPath();
const std::string KEY = getKeyPath();
const std::string MODULES_DIR = std::string { CTHUN_AGENT_ROOT_PATH } + "/test_modules";
const std::string SPOOL_DIR = std::string { CTHUN_AGENT_ROOT_PATH } + "/test_spool/";

const char* ARGV[] = { "test-command",
                       "--server", SERVER.data(),
                       "--ca", CA.data(),
                       "--cert", CERT.data(),
                       "--key", KEY.data(),
                       "--modules-dir", MODULES_DIR.data(),
                       "--spool-dir", SPOOL_DIR.data() };
const int ARGC = 13;

static void configureTest() {
    Configuration::Instance().initialize(ARGC, const_cast<char**>(ARGV));
}

static void resetTest() {
    Configuration::Instance().reset();
}

TEST_CASE("Configuration - metatest", "[configuration]") {
    resetTest();

    SECTION("Metatest - can initialize Configuration") {
        REQUIRE_NOTHROW(Configuration::Instance()
                            .initialize(ARGC, const_cast<char**>(ARGV)));
    }

    configureTest();

    SECTION("Metatest - we can inject CL options into HorseWhisperer") {
        REQUIRE(HW::GetFlag<std::string>("ca") == CA);
        REQUIRE(HW::GetFlag<std::string>("modules-dir") == MODULES_DIR);
        REQUIRE(HW::GetFlag<std::string>("spool-dir") == SPOOL_DIR);
    }

    resetTest();
}

TEST_CASE("Configuration::setStartFunction", "[configuration]") {
    resetTest();

    SECTION("No error when starting without setting a function") {
        configureTest();

        REQUIRE_NOTHROW(HW::Start());
    }

    SECTION("It does start correctly") {
        Configuration::Instance().setStartFunction(
            [] (std::vector<std::string> arg) -> int {
                return 1;
            });
        configureTest();

        REQUIRE(HW::Start() == 1);
    }

    resetTest();
}

TEST_CASE("Configuration::set", "[configuration]") {
    configureTest();

    SECTION("can set a known flag to a valid value") {
        REQUIRE_NOTHROW(Configuration::Instance()
                            .set<std::string>("ca", "value"));
    }

    SECTION("set a flag correctly") {
        Configuration::Instance().set<std::string>("ca", "value");
        REQUIRE(HW::GetFlag<std::string>("ca") == "value");
    }

    SECTION("throw when setting an unknown flag") {
        REQUIRE_THROWS_AS(Configuration::Instance()
                                .set<int>("tentacle_spawning_interval_s", 45),
                          Configuration::ConfigurationEntryError);
    }

    SECTION("throw when setting a known flag to an invalid value") {
        HW::DefineGlobalFlag<int>("num_tentacles",
                                  "number of spawn tentacles",
                                  8,
                                  [](int v) {
                                      if (v != 8) {
                                          throw HW::flag_validation_error { "bad!" };
                                      }
                                  });

        // The only valid number of tentacles is 8
        REQUIRE_THROWS_AS(Configuration::Instance().set<int>("num_tentacles", 42),
                          Configuration::ConfigurationEntryError);
        REQUIRE_NOTHROW(Configuration::Instance().set<int>("num_tentacles", 8));
    }

    resetTest();
}

TEST_CASE("Configuration::get", "[configuration]") {
    SECTION("Configuration was not initialized yet") {
        SECTION("can get a defined flag") {
            REQUIRE_NOTHROW(Configuration::Instance().get<std::string>("ca"));
        }

        SECTION("return the default value correctly") {
            REQUIRE(Configuration::Instance().get<std::string>("spool-dir")
                    == DEFAULT_ACTION_RESULTS_DIR);
        }

        SECTION("throw a Configuration::ConfigurationEntryError if the flag "
                "is unknown") {
            REQUIRE_THROWS_AS(
                Configuration::Instance().get<std::string>("dont_exist"),
                Configuration::ConfigurationEntryError);
        }
    }

    SECTION("Configuration was initialized") {
        configureTest();

        SECTION("can get a defined flag") {
            REQUIRE_NOTHROW(Configuration::Instance().get<std::string>("ca"));
        }

        SECTION("return the default value if the flag was not set") {
            resetTest();
            // NB: ignoring --spool-dir in ARGV since argc is set to 9
            Configuration::Instance().initialize(9, const_cast<char**>(ARGV));

            REQUIRE(Configuration::Instance().get<std::string>("spool-dir")
                    == DEFAULT_ACTION_RESULTS_DIR);
        }

        SECTION("return the correct value after the flag has been set") {
            Configuration::Instance().set<std::string>("spool-dir", "/fake/dir");
            REQUIRE(Configuration::Instance().get<std::string>("spool-dir")
                    == "/fake/dir");
        }

        SECTION("throw a configuration error if the flag is unknown") {
            REQUIRE_THROWS_AS(
                Configuration::Instance().get<std::string>("still_dont_exist"),
                Configuration::ConfigurationEntryError);
        }

        resetTest();
    }
}

TEST_CASE("Configuration::validateAndNormalizeConfiguration", "[configuration]") {
    configureTest();

    SECTION("it fails when server is undefined") {
        Configuration::Instance().set<std::string>("server", "");
        REQUIRE_THROWS_AS(Configuration::Instance().validateAndNormalizeConfiguration(),
                          Configuration::RequiredNotSetError);
    }

    SECTION("it fails when server is invlaid") {
        Configuration::Instance().set<std::string>("server", "ws://");
        REQUIRE_THROWS_AS(Configuration::Instance().validateAndNormalizeConfiguration(),
                          Configuration::ConfigurationEntryError);
    }

    SECTION("it fails when ca is undefined") {
        Configuration::Instance().set<std::string>("ca", "");
        REQUIRE_THROWS_AS(Configuration::Instance().validateAndNormalizeConfiguration(),
                          Configuration::RequiredNotSetError);
    }

    SECTION("it fails when ca file cannot be found") {
        Configuration::Instance().set<std::string>("ca", "/fake/file");
        REQUIRE_THROWS_AS(Configuration::Instance().validateAndNormalizeConfiguration(),
                          Configuration::ConfigurationEntryError);
    }

    SECTION("it fails when cert is undefined") {
        Configuration::Instance().set<std::string>("cert", "");
        REQUIRE_THROWS_AS(Configuration::Instance().validateAndNormalizeConfiguration(),
                          Configuration::RequiredNotSetError);
    }

    SECTION("it fails when cert file cannot be found") {
        Configuration::Instance().set<std::string>("cert", "/fake/file");
        REQUIRE_THROWS_AS(Configuration::Instance().validateAndNormalizeConfiguration(),
                          Configuration::ConfigurationEntryError);
    }

    SECTION("it fails when key is undefined") {
        Configuration::Instance().set<std::string>("key", "");
        REQUIRE_THROWS_AS(Configuration::Instance().validateAndNormalizeConfiguration(),
                          Configuration::RequiredNotSetError);
    }

    SECTION("it fails when key file cannot be found") {
        Configuration::Instance().set<std::string>("key", "/fake/file");
        REQUIRE_THROWS_AS(Configuration::Instance().validateAndNormalizeConfiguration(),
                          Configuration::ConfigurationEntryError);
    }

    SECTION("it fails when spool-dir is empty") {
        Configuration::Instance().set<std::string>("spool-dir", "");
        REQUIRE_THROWS_AS(Configuration::Instance().validateAndNormalizeConfiguration(),
                          Configuration::RequiredNotSetError);
    }
    resetTest();
}

}  // namespace CthunAgent
