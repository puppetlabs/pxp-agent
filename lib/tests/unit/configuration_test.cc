#include "certs.hpp"
#include "root_path.hpp"

#include <pxp-agent/configuration.hpp>

#include "horsewhisperer/horsewhisperer.h"

#include <leatherman/util/scope_exit.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.configuration_test"
#include <leatherman/logging/logging.hpp>

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include <catch.hpp>

#include <string>
#include <vector>

namespace PXPAgent {

namespace fs = boost::filesystem;
namespace HW = HorseWhisperer;
namespace lth_log = leatherman::logging;
namespace lth_util = leatherman::util;

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
    "--foreground=true",
    nullptr };
static const int ARGC = 16;

static void configureTest() {
    if (!fs::exists(SPOOL_DIR) && !fs::create_directories(SPOOL_DIR)) {
        FAIL("Failed to create the results directory");
    }
    Configuration::Instance().initialize(
        [](std::vector<std::string>) {
            return EXIT_SUCCESS;
        });
    // Configuration::Instance().initialize(ARGC, const_cast<char**>(ARGV), false);
}

static void resetTest() {
    if (fs::exists(SPOOL_DIR)) {
        fs::remove_all(SPOOL_DIR);
    }
}

TEST_CASE("Configuration - metatest", "[configuration]") {
    lth_util::scope_exit config_cleaner { resetTest };

    SECTION("Metatest - can initialize Configuration") {
        resetTest();
        REQUIRE_NOTHROW(configureTest());
    }

    SECTION("Metatest - we can inject CL options into HorseWhisperer") {
        configureTest();
        Configuration::Instance().parseOptions(ARGC, const_cast<char**>(ARGV));
        REQUIRE(HW::GetFlag<std::string>("ssl-ca-cert") == CA);
        REQUIRE(HW::GetFlag<std::string>("modules-dir") == MODULES_DIR);
        REQUIRE(HW::GetFlag<std::string>("spool-dir") == SPOOL_DIR);
    }
}

TEST_CASE("Configuration::initialize()", "[configuration]") {
    lth_util::scope_exit config_cleaner { resetTest };

    SECTION("No error when starting without setting a function") {
        REQUIRE_NOTHROW(HW::Start());
    }

    SECTION("It does set HorseWhisperer's action correctly") {
        Configuration::Instance().initialize(
            [] (std::vector<std::string> arg) -> int {
                return 42;
            });
        const char* args[] = { "pxp-agent", "start", nullptr };
        HW::Parse(2, const_cast<char**>(args));

        REQUIRE(HW::Start() == 42);
    }
}

TEST_CASE("Configuration::set", "[configuration]") {
    configureTest();
    Configuration::Instance().parseOptions(ARGC, const_cast<char**>(ARGV));
    Configuration::Instance().validate();
    lth_util::scope_exit config_cleaner { resetTest };

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
}

TEST_CASE("Configuration::get", "[configuration]") {
    lth_util::scope_exit config_cleaner { resetTest };
    configureTest();

    SECTION("When options were not parsed and validated yet") {
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

    SECTION("After parsing and validating options") {
        configureTest();
        Configuration::Instance().parseOptions(ARGC, const_cast<char**>(ARGV));
        Configuration::Instance().validate();

        SECTION("can get a defined flag") {
            REQUIRE_NOTHROW(Configuration::Instance().get<std::string>("ssl-ca-cert"));
        }

        SECTION("return the default value if the flag was not set") {
            // NB: ignoring --foreground in ARGV since argc is set to 15
            configureTest();
            Configuration::Instance().parseOptions(15, const_cast<char**>(ARGV));
            Configuration::Instance().validate();

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
    }
}

TEST_CASE("Configuration::validate", "[configuration]") {
    lth_util::scope_exit config_cleaner { resetTest };
    configureTest();
    Configuration::Instance().parseOptions(ARGC, const_cast<char**>(ARGV));

    SECTION("it throws an UnconfiguredError when the broker WebSocket URI is undefined") {
        HW::SetFlag<std::string>("broker-ws-uri", "");
        REQUIRE_THROWS_AS(Configuration::Instance().validate(),
                          Configuration::UnconfiguredError);
    }

    SECTION("it throws an UnconfiguredError when the broker WebSocket URi is invlaid") {
        HW::SetFlag<std::string>("broker-ws-uri", "ws://");
        REQUIRE_THROWS_AS(Configuration::Instance().validate(),
                          Configuration::UnconfiguredError);
    }

    SECTION("it throws an UnconfiguredError ssl-ca-cert is undefined") {
        HW::SetFlag<std::string>("ssl-ca-cert", "");
        REQUIRE_THROWS_AS(Configuration::Instance().validate(),
                          Configuration::UnconfiguredError);
    }

    SECTION("it throws an UnconfiguredError when ssl-ca-cert file cannot be found") {
        HW::SetFlag<std::string>("ssl-ca-cert", "/fake/file");
        REQUIRE_THROWS_AS(Configuration::Instance().validate(),
                          Configuration::UnconfiguredError);
    }

    SECTION("it throws an UnconfiguredError when ssl-cert is undefined") {
        HW::SetFlag<std::string>("ssl-cert", "");
        REQUIRE_THROWS_AS(Configuration::Instance().validate(),
                          Configuration::UnconfiguredError);
    }

    SECTION("it throws an UnconfiguredError when ssl-cert file cannot be found") {
        HW::SetFlag<std::string>("ssl-cert", "/fake/file");
        REQUIRE_THROWS_AS(Configuration::Instance().validate(),
                          Configuration::UnconfiguredError);
    }

    SECTION("it throws an UnconfiguredError when ssl-key is undefined") {
        HW::SetFlag<std::string>("ssl-key", "");
        REQUIRE_THROWS_AS(Configuration::Instance().validate(),
                          Configuration::UnconfiguredError);
    }

    SECTION("it throws an UnconfiguredError when ssl-key file cannot be found") {
        HW::SetFlag<std::string>("ssl-key", "/fake/file");
        REQUIRE_THROWS_AS(Configuration::Instance().validate(),
                          Configuration::UnconfiguredError);
    }

    SECTION("it fails when spool-dir is empty") {
        HW::SetFlag<std::string>("spool-dir", "");
        REQUIRE_THROWS_AS(Configuration::Instance().validate(),
                          Configuration::Error);
    }
}

TEST_CASE("Configuration::setupLogging", "[configuration]") {
    lth_util::scope_exit config_cleaner { resetTest };
    configureTest();
    Configuration::Instance().parseOptions(ARGC, const_cast<char**>(ARGV));
    Configuration::Instance().validate();

    SECTION("it sets log level to info by default") {
        Configuration::Instance().setupLogging();
        REQUIRE(lth_log::get_level() == lth_log::log_level::info);
    }

#ifndef _WIN32
    SECTION("it sets log level to none if foreground is unflagged and logfile "
            "is set to stdout") {
        Configuration::Instance().set<bool>("foreground", false);
        Configuration::Instance().set<std::string>("logfile", "-");
        Configuration::Instance().setupLogging();

        REQUIRE(lth_log::get_level() == lth_log::log_level::none);
    }
#endif
}

}  // namespace PXPAgent
