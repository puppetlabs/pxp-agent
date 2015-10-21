#include "certs.hpp"
#include "root_path.hpp"

#include <pxp-agent/configuration.hpp>

#include "horsewhisperer/horsewhisperer.h"

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.configuration_test"
#include <leatherman/logging/logging.hpp>

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include <catch.hpp>

#include <string>

namespace PXPAgent {

namespace fs = boost::filesystem;
namespace HW = HorseWhisperer;
namespace lth_log = leatherman::logging;

static const std::string CONFIG { std::string { PXP_AGENT_ROOT_PATH }
                                  + "/lib/tests/resources/config/empty-pxp-agent.cfg" };
static const std::string TEST_BROKER_WS_URI { "wss:///test_c_t_h_u_n_broker" };
static const std::string CA { getCaPath() };
static const std::string CERT { getCertPath() };
static const std::string KEY { getKeyPath() };
static const std::string MODULES_DIR { std::string { PXP_AGENT_ROOT_PATH }
                                       + "/lib/tests/resources/test_modules/" };
static const std::string SPOOL_DIR { std::string { PXP_AGENT_ROOT_PATH }
                                     + "/lib/tests/resources/test_spool" };

static const char* ARGV[] = {
    "test-command",
    "--config-file", CONFIG.data(),
    "--broker-ws-uri", TEST_BROKER_WS_URI.data(),
    "--ssl-ca-cert", CA.data(),
    "--ssl-cert", CERT.data(),
    "--ssl-key", KEY.data(),
    "--modules-dir", MODULES_DIR.data(),
    "--spool-dir", SPOOL_DIR.data(),
    "--foreground=true"};
static const int ARGC = 16;

static void configureTest() {
    HW::Reset();
    if (!fs::exists(SPOOL_DIR) && !fs::create_directories(SPOOL_DIR)) {
        FAIL("Failed to create the results directory");
    }
    Configuration::Instance().initialize(ARGC, const_cast<char**>(ARGV), false);
}

static void resetTest() {
    Configuration::Instance().reset();
    fs::remove_all(SPOOL_DIR);
}

TEST_CASE("Configuration - metatest", "[configuration]") {
    SECTION("Metatest - can initialize Configuration") {
        resetTest();
        configureTest();
        REQUIRE_NOTHROW(Configuration::Instance()
                            .initialize(ARGC, const_cast<char**>(ARGV), false));
    }


    SECTION("Metatest - we can inject CL options into HorseWhisperer") {
        configureTest();
        REQUIRE(HW::GetFlag<std::string>("ssl-ca-cert") == CA);
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
        REQUIRE_NOTHROW(Configuration::Instance().set<std::string>("ssl-ca-cert",
                                                                   "value"));
    }

    SECTION("set a flag correctly") {
        Configuration::Instance().set<std::string>("ssl-ca-cert", "value");
        REQUIRE(HW::GetFlag<std::string>("ssl-ca-cert") == "value");
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
            REQUIRE_NOTHROW(Configuration::Instance().get<std::string>("ssl-ca-cert"));
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
            REQUIRE_NOTHROW(Configuration::Instance().get<std::string>("ssl-ca-cert"));
        }

        SECTION("return the default value if the flag was not set") {
            // NB: ignoring --foreground in ARGV since argc is set to 15
            Configuration::Instance().initialize(15, const_cast<char**>(ARGV), false);

            REQUIRE(Configuration::Instance().get<bool>("foreground") == false);
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

    SECTION("it fails when ssl-ca-cert is undefined") {
        Configuration::Instance().set<std::string>("ssl-ca-cert", "");
        REQUIRE_THROWS_AS(Configuration::Instance().validateAndNormalizeConfiguration(),
                          Configuration::Error);
    }

    SECTION("it fails when ssl-ca-cert file cannot be found") {
        Configuration::Instance().set<std::string>("ssl-ca-cert", "/fake/file");
        REQUIRE_THROWS_AS(Configuration::Instance().validateAndNormalizeConfiguration(),
                          Configuration::Error);
    }

    SECTION("it fails when ssl-cert is undefined") {
        Configuration::Instance().set<std::string>("ssl-cert", "");
        REQUIRE_THROWS_AS(Configuration::Instance().validateAndNormalizeConfiguration(),
                          Configuration::Error);
    }

    SECTION("it fails when ssl-cert file cannot be found") {
        Configuration::Instance().set<std::string>("ssl-cert", "/fake/file");
        REQUIRE_THROWS_AS(Configuration::Instance().validateAndNormalizeConfiguration(),
                          Configuration::Error);
    }

    SECTION("it fails when ssl-key is undefined") {
        Configuration::Instance().set<std::string>("ssl-key", "");
        REQUIRE_THROWS_AS(Configuration::Instance().validateAndNormalizeConfiguration(),
                          Configuration::Error);
    }

    SECTION("it fails when ssl-key file cannot be found") {
        Configuration::Instance().set<std::string>("ssl-key", "/fake/file");
        REQUIRE_THROWS_AS(Configuration::Instance().validateAndNormalizeConfiguration(),
                          Configuration::Error);
    }

    SECTION("it fails when spool-dir is empty") {
        Configuration::Instance().set<std::string>("spool-dir", "");
        REQUIRE_THROWS_AS(Configuration::Instance().validateAndNormalizeConfiguration(),
                          Configuration::Error);
    }

    SECTION("it sets log level to none if foreground is unflagged and logfile is set to stdout") {
        Configuration::Instance().set<bool>("foreground", false);
        Configuration::Instance().set<std::string>("logfile", "-");

        REQUIRE(lth_log::get_level() == lth_log::log_level::none);
    }

    resetTest();
}

}  // namespace PXPAgent
