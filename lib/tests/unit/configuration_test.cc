#include "certs.hpp"

#include "root_path.hpp"

#include <pxp-agent/configuration.hpp>

#include "horsewhisperer/horsewhisperer.h"

#include <catch.hpp>

#include <string>

namespace PXPAgent {

namespace HW = HorseWhisperer;

const std::string CONFIG { std::string { PXP_AGENT_ROOT_PATH }
                           + "/lib/tests/resources/config/empty-pxp-agent.cfg" };
const std::string TEST_BROKER_WS_URI { "wss:///test_c_t_h_u_n_broker" };
const std::string CA { getCaPath() };
const std::string CERT { getCertPath() };
const std::string KEY { getKeyPath() };
const std::string MODULES_DIR { std::string { PXP_AGENT_ROOT_PATH }
                                + "/lib/tests/resources/test_modules/" };
const std::string SPOOL_DIR { std::string { PXP_AGENT_ROOT_PATH }
                              + "/lib/tests/resources/test_spool/" };

const char* ARGV[] = { "test-command",
                       "--config-file", CONFIG.data(),
                       "--broker-ws-uri", TEST_BROKER_WS_URI.data(),
                       "--ca", CA.data(),
                       "--cert", CERT.data(),
                       "--key", KEY.data(),
                       "--modules-dir", MODULES_DIR.data(),
                       "--spool-dir", SPOOL_DIR.data(),
                       "--foreground=true"};
const int ARGC = 16;

static void configureTest() {
    Configuration::Instance().initialize(ARGC, const_cast<char**>(ARGV), false);
}

static void resetTest() {
    Configuration::Instance().reset();
}

TEST_CASE("Configuration - metatest", "[configuration]") {
    resetTest();

    SECTION("Metatest - can initialize Configuration") {
        REQUIRE_NOTHROW(Configuration::Instance()
                            .initialize(ARGC, const_cast<char**>(ARGV), false));
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
                          Configuration::Error);
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
                          Configuration::Error);
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
                    == DEFAULT_SPOOL_DIR);
        }

        SECTION("throw a Configuration::Error if the flag is unknown") {
            REQUIRE_THROWS_AS(
                Configuration::Instance().get<std::string>("dont_exist"),
                Configuration::Error);
        }
    }

    SECTION("Configuration was initialized") {
        configureTest();

        SECTION("can get a defined flag") {
            REQUIRE_NOTHROW(Configuration::Instance().get<std::string>("ca"));
        }

        SECTION("return the default value if the flag was not set") {
            resetTest();
            // NB: ignoring --foreground in ARGV since argc is set to 14
            Configuration::Instance().initialize(15, const_cast<char**>(ARGV), false);

            REQUIRE(Configuration::Instance().get<bool>("foreground")
                    == false);
        }

        SECTION("return the correct value after the flag has been set") {
            Configuration::Instance().set<std::string>("spool-dir", "/fake/dir");
            REQUIRE(Configuration::Instance().get<std::string>("spool-dir")
                    == "/fake/dir");
        }

        SECTION("throw a Configuration::Error if the flag is unknown") {
            REQUIRE_THROWS_AS(
                Configuration::Instance().get<std::string>("still_dont_exist"),
                Configuration::Error);
        }

        resetTest();
    }
}

TEST_CASE("Configuration::validateAndNormalizeConfiguration", "[configuration]") {
    configureTest();

    SECTION("it fails when the broker WebSocket URI is undefined") {
        Configuration::Instance().set<std::string>("broker-ws-uri", "");
        REQUIRE_THROWS_AS(Configuration::Instance().validateAndNormalizeConfiguration(),
                          Configuration::Error);
    }

    SECTION("it fails when the broker WebSocket URi is invlaid") {
        Configuration::Instance().set<std::string>("broker-ws-uri", "ws://");
        REQUIRE_THROWS_AS(Configuration::Instance().validateAndNormalizeConfiguration(),
                          Configuration::Error);
    }

    SECTION("it fails when ca is undefined") {
        Configuration::Instance().set<std::string>("ca", "");
        REQUIRE_THROWS_AS(Configuration::Instance().validateAndNormalizeConfiguration(),
                          Configuration::Error);
    }

    SECTION("it fails when ca file cannot be found") {
        Configuration::Instance().set<std::string>("ca", "/fake/file");
        REQUIRE_THROWS_AS(Configuration::Instance().validateAndNormalizeConfiguration(),
                          Configuration::Error);
    }

    SECTION("it fails when cert is undefined") {
        Configuration::Instance().set<std::string>("cert", "");
        REQUIRE_THROWS_AS(Configuration::Instance().validateAndNormalizeConfiguration(),
                          Configuration::Error);
    }

    SECTION("it fails when cert file cannot be found") {
        Configuration::Instance().set<std::string>("cert", "/fake/file");
        REQUIRE_THROWS_AS(Configuration::Instance().validateAndNormalizeConfiguration(),
                          Configuration::Error);
    }

    SECTION("it fails when key is undefined") {
        Configuration::Instance().set<std::string>("key", "");
        REQUIRE_THROWS_AS(Configuration::Instance().validateAndNormalizeConfiguration(),
                          Configuration::Error);
    }

    SECTION("it fails when key file cannot be found") {
        Configuration::Instance().set<std::string>("key", "/fake/file");
        REQUIRE_THROWS_AS(Configuration::Instance().validateAndNormalizeConfiguration(),
                          Configuration::Error);
    }

    SECTION("it fails when spool-dir is empty") {
        Configuration::Instance().set<std::string>("spool-dir", "");
        REQUIRE_THROWS_AS(Configuration::Instance().validateAndNormalizeConfiguration(),
                          Configuration::Error);
    }

    SECTION("it fails when foreground is unflagged and log is set to console") {
        Configuration::Instance().set<bool>("foreground", false);
        Configuration::Instance().set<bool>("console-logger", true);
        REQUIRE_THROWS_AS(Configuration::Instance().validateAndNormalizeConfiguration(),
                          Configuration::Error);
    }

    resetTest();
}

}  // namespace PXPAgent
