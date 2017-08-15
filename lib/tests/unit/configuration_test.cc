#include "../common/certs.hpp"
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
static const std::string MULTI_BROKER_CONFIG { std::string { PXP_AGENT_ROOT_PATH }
                                               + "/lib/tests/resources/config/multi-broker.cfg" };
static const std::string DUPLICATE_CONFIG { std::string { PXP_AGENT_ROOT_PATH }
                                            + "/lib/tests/resources/config/duplicate.cfg" };
static const std::string BAD_BROKER_CONFIG { std::string { PXP_AGENT_ROOT_PATH }
                                          + "/lib/tests/resources/config/bad-broker.cfg" };
static const std::string BAD_MASTER_CONFIG { std::string { PXP_AGENT_ROOT_PATH }
                                          + "/lib/tests/resources/config/bad-master.cfg" };
static const std::string TEST_BROKER_WS_URI { "wss:///test_c_t_h_u_n_broker" };
static const std::string CA { getCaPath() };
static const std::string CERT { getCertPath() };
static const std::string KEY { getKeyPath() };
static const std::string MODULES_DIR { std::string { PXP_AGENT_ROOT_PATH }
                                       + "/lib/tests/resources/modules/" };
static const std::string MODULES_CONFIG_DIR { std::string { PXP_AGENT_ROOT_PATH }
                                       + "/lib/tests/resources/modules_config" };
static const std::string SPOOL_DIR { std::string { PXP_AGENT_ROOT_PATH }
                                     + "/lib/tests/resources/test_spool" };

static const char* ARGV[] = {
    "test-command",
    "--config-file", CONFIG.data(),
    "--broker-ws-uri", TEST_BROKER_WS_URI.data(),
    "--pcp-version=2",
    "--ssl-ca-cert", CA.data(),
    "--ssl-cert", CERT.data(),
    "--ssl-key", KEY.data(),
    "--modules-dir", MODULES_DIR.data(),
    "--modules-config-dir", MODULES_CONFIG_DIR.data(),
    "--spool-dir", SPOOL_DIR.data(),
    "--foreground=true",
    nullptr };
static const int ARGC = 19;

static void configureTest() {
    if (!fs::exists(SPOOL_DIR) && !fs::create_directories(SPOOL_DIR)) {
        FAIL("Failed to create the results directory");
    }
    if (!fs::exists(MODULES_CONFIG_DIR) && !fs::create_directories(MODULES_CONFIG_DIR)) {
        FAIL("Failed to create the modules configuration directory");
    }
    Configuration::Instance().initialize(
        [](std::vector<std::string>) {
            return EXIT_SUCCESS;
        });
}

static void resetTest() {
    if (fs::exists(SPOOL_DIR)) {
        fs::remove_all(SPOOL_DIR);
    }
    if (fs::exists(MODULES_CONFIG_DIR)) {
        fs::remove_all(MODULES_CONFIG_DIR);
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

        SECTION("can get a defined flag") {
            Configuration::Instance().parseOptions(ARGC, const_cast<char**>(ARGV));
            Configuration::Instance().validate();
            REQUIRE_NOTHROW(Configuration::Instance().get<std::string>("ssl-ca-cert"));
        }

        SECTION("return the default value if the flag was not set") {
            // NB: ignoring --foreground in ARGV since argc is set to 19
            Configuration::Instance().parseOptions(18, const_cast<char**>(ARGV));
#ifndef _WIN32
            HW::SetFlag<std::string>("pidfile", SPOOL_DIR + "/test.pid");
#endif
            Configuration::Instance().validate();

            REQUIRE_FALSE(Configuration::Instance().get<bool>("foreground"));
        }

        SECTION("return the correct value after the flag has been set") {
            Configuration::Instance().parseOptions(ARGC, const_cast<char**>(ARGV));
            Configuration::Instance().validate();
            Configuration::Instance().set<std::string>("spool-dir", "/fake/dir");
            REQUIRE(Configuration::Instance().get<std::string>("spool-dir")
                    == "/fake/dir");
        }

        SECTION("throw a Configuration::Error if the flag is unknown") {
            Configuration::Instance().parseOptions(ARGC, const_cast<char**>(ARGV));
            Configuration::Instance().validate();
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

    SECTION("it throws an Error when the broker WebSocket URI is undefined") {
        HW::SetFlag<std::string>("broker-ws-uri", "");
        REQUIRE_THROWS_AS(Configuration::Instance().validate(),
                          Configuration::Error);
    }

    SECTION("it throws an Error when the broker WebSocket URi is invalid") {
        HW::SetFlag<std::string>("broker-ws-uri", "ws://");
        REQUIRE_THROWS_AS(Configuration::Instance().validate(),
                          Configuration::Error);
    }

    SECTION("it throws an Error when the PCP version is invalid") {
        HW::SetFlag<std::string>("pcp-version", "3");
        REQUIRE_THROWS_AS(Configuration::Instance().validate(),
                          Configuration::Error);
    }

    SECTION("it throws an Error ssl-ca-cert is undefined") {
        HW::SetFlag<std::string>("ssl-ca-cert", "");
        REQUIRE_THROWS_AS(Configuration::Instance().validate(),
                          Configuration::Error);
    }

    SECTION("it throws an Error when ssl-ca-cert file cannot be found") {
        HW::SetFlag<std::string>("ssl-ca-cert", "/fake/file");
        REQUIRE_THROWS_AS(Configuration::Instance().validate(),
                          Configuration::Error);
    }

    SECTION("it throws an Error when ssl-ca-cert file is not "
            "readable (is a directory)") {
        HW::SetFlag<std::string>("ssl-ca-cert", "/fake");
        REQUIRE_THROWS_AS(Configuration::Instance().validate(),
                          Configuration::Error);
    }

    SECTION("it throws an Error when ssl-cert is undefined") {
        HW::SetFlag<std::string>("ssl-cert", "");
        REQUIRE_THROWS_AS(Configuration::Instance().validate(),
                          Configuration::Error);
    }

    SECTION("it throws an Error when ssl-cert file cannot be found") {
        HW::SetFlag<std::string>("ssl-cert", "/fake/file");
        REQUIRE_THROWS_AS(Configuration::Instance().validate(),
                          Configuration::Error);
    }

    SECTION("it throws an Error when ssl-cert file is not "
            "readable (is a directory)") {
        HW::SetFlag<std::string>("ssl-cert", "/fake");
        REQUIRE_THROWS_AS(Configuration::Instance().validate(),
                          Configuration::Error);
    }

    SECTION("it throws an Error when ssl-key is undefined") {
        HW::SetFlag<std::string>("ssl-key", "");
        REQUIRE_THROWS_AS(Configuration::Instance().validate(),
                          Configuration::Error);
    }

    SECTION("it throws an Error when ssl-key file cannot be found") {
        HW::SetFlag<std::string>("ssl-key", "/fake/file");
        REQUIRE_THROWS_AS(Configuration::Instance().validate(),
                          Configuration::Error);
    }

    SECTION("it throws an Error when ssl-key file is not "
            "readable (is a directory)") {
        HW::SetFlag<std::string>("ssl-key", "/fake");
        REQUIRE_THROWS_AS(Configuration::Instance().validate(),
                          Configuration::Error);
    }

    SECTION("it fails when --modules-dir does not exist") {
        auto test_modules = SPOOL_DIR + "/testing_modules";
        HW::SetFlag<std::string>("modules-dir", test_modules);
        REQUIRE_THROWS_AS(Configuration::Instance().validate(),
                          Configuration::Error);
    }

    SECTION("it fails when --modules-config-dir is not a directory") {
        HW::SetFlag<std::string>("modules-dir", CONFIG);
        REQUIRE_THROWS_AS(Configuration::Instance().validate(),
                          Configuration::Error);
    }

        SECTION("it fails when --spool-dir is empty") {
        HW::SetFlag<std::string>("spool-dir", "");
        REQUIRE_THROWS_AS(Configuration::Instance().validate(),
                          Configuration::Error);
    }

    SECTION("it fails when --spool-dir exists but is not a directory") {
        HW::SetFlag<std::string>("spool-dir", CONFIG);
        REQUIRE_THROWS_AS(Configuration::Instance().validate(),
                          Configuration::Error);
    }

    SECTION("it creates --spool-dir when needed") {
        auto test_spool = SPOOL_DIR + "/testing_creation";
        HW::SetFlag<std::string>("spool-dir", test_spool);

        REQUIRE_FALSE(fs::exists(test_spool));
        REQUIRE_NOTHROW(Configuration::Instance().validate());
        REQUIRE(fs::exists(test_spool));

        fs::remove_all(test_spool);
    }
}

TEST_CASE("Configuration::validate multiple brokers", "[configuration]") {
    const char* altArgv[] = {
    "test-command",
    "--config-file", MULTI_BROKER_CONFIG.data(),
    "--ssl-ca-cert", CA.data(),
    "--ssl-cert", CERT.data(),
    "--ssl-key", KEY.data(),
    "--modules-dir", MODULES_DIR.data(),
    "--modules-config-dir", MODULES_CONFIG_DIR.data(),
    "--spool-dir", SPOOL_DIR.data(),
    "--foreground=true",
    nullptr };
    const int altArgc = 16;

    lth_util::scope_exit config_cleaner { resetTest };
    configureTest();
    Configuration::Instance().parseOptions(altArgc, const_cast<char**>(altArgv));

    SECTION("it allows broker-ws-uris in place of broker-ws-uri") {
        HW::SetFlag<std::string>("broker-ws-uri", TEST_BROKER_WS_URI);
        REQUIRE_NOTHROW(Configuration::Instance().validate());
        auto uris = Configuration::Instance().get_broker_ws_uris();
        REQUIRE(uris == std::vector<std::string>({TEST_BROKER_WS_URI}));
    }

    SECTION("it parses multiple arguments to broker-ws-uris") {
        REQUIRE_NOTHROW(Configuration::Instance().validate());
        auto uris = Configuration::Instance().get_broker_ws_uris();
        REQUIRE(uris == std::vector<std::string>({"wss://test_pcp_broker", "wss://alt_pcp_broker"}));
    }

    SECTION("it parses multiple arguments to master-uris") {
        REQUIRE_NOTHROW(Configuration::Instance().validate());
        auto uris = Configuration::Instance().get_master_uris();
        REQUIRE(uris == std::vector<std::string>({"https://test_master:8140", "https://alt_master:8140"}));
    }
}

TEST_CASE("Configuration::parseOptions duplicate broker-ws-uris", "[configuration]") {
    const char* altArgv[] = {
    "test-command",
    "--config-file", DUPLICATE_CONFIG.data(),
    "--ssl-ca-cert", CA.data(),
    "--ssl-cert", CERT.data(),
    "--ssl-key", KEY.data(),
    "--modules-dir", MODULES_DIR.data(),
    "--modules-config-dir", MODULES_CONFIG_DIR.data(),
    "--spool-dir", SPOOL_DIR.data(),
    "--foreground=true",
    nullptr };
    const int altArgc = 16;

    lth_util::scope_exit config_cleaner { resetTest };
    configureTest();

    SECTION("it throws an Error when broker-ws-uri and broker-ws-uris are defined") {
        REQUIRE_THROWS_AS(Configuration::Instance().parseOptions(altArgc, const_cast<char**>(altArgv)),
                          Configuration::Error);
    }
}

TEST_CASE("Configuration::validate bad broker-ws-uris", "[configuration]") {
    const char* altArgv[] = {
    "test-command",
    "--config-file", BAD_BROKER_CONFIG.data(),
    "--ssl-ca-cert", CA.data(),
    "--ssl-cert", CERT.data(),
    "--ssl-key", KEY.data(),
    "--modules-dir", MODULES_DIR.data(),
    "--modules-config-dir", MODULES_CONFIG_DIR.data(),
    "--spool-dir", SPOOL_DIR.data(),
    "--foreground=true",
    nullptr };
    const int altArgc = 16;

    lth_util::scope_exit config_cleaner { resetTest };
    configureTest();
    Configuration::Instance().parseOptions(altArgc, const_cast<char**>(altArgv));

    SECTION("it throws an Error when broker-ws-uris is invalid") {
        REQUIRE_THROWS_AS(Configuration::Instance().validate(),
                          Configuration::Error);
    }
}

TEST_CASE("Configuration::validate bad master-uris", "[configuration]") {
    const char* altArgv[] = {
    "test-command",
    "--config-file", BAD_MASTER_CONFIG.data(),
    "--ssl-ca-cert", CA.data(),
    "--ssl-cert", CERT.data(),
    "--ssl-key", KEY.data(),
    "--modules-dir", MODULES_DIR.data(),
    "--modules-config-dir", MODULES_CONFIG_DIR.data(),
    "--spool-dir", SPOOL_DIR.data(),
    "--foreground=true",
    nullptr };
    const int altArgc = 16;

    lth_util::scope_exit config_cleaner { resetTest };
    configureTest();
    Configuration::Instance().parseOptions(altArgc, const_cast<char**>(altArgv));

    SECTION("it throws an Error when master-uris is invalid") {
        REQUIRE_THROWS_AS(Configuration::Instance().validate(),
                          Configuration::Error);
    }
}

TEST_CASE("Configuration::setupLogging", "[configuration]") {
    lth_util::scope_exit config_cleaner { resetTest };
    configureTest();
    Configuration::Instance().parseOptions(ARGC, const_cast<char**>(ARGV));
    Configuration::Instance().validate();

#ifndef _WIN32
    SECTION("it sets log level to info by default") {
        // NOTE(ale): we cannot test this on Windows because it won't
        // allow us to delete the SPOOL_DIR while test.log is open
        Configuration::Instance().set<std::string>("logfile", SPOOL_DIR + "/test.log");
        Configuration::Instance().setupLogging();
        REQUIRE(lth_log::get_level() == lth_log::log_level::info);
    }

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
