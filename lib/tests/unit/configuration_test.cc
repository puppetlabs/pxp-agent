#include "test/test.hpp"
#include <catch.hpp>

#include <cthun-agent/configuration.hpp>
#include <cthun-agent/errors.hpp>

#include "horsewhisperer/horsewhisperer.h"

extern std::string ROOT_PATH;

namespace CthunAgent {

void configureTest() {
    std::string server = "wss://test_server/";
    std::string ca = ROOT_PATH + "/lib/tests/resources/config/ca_crt.pem";
    std::string cert = ROOT_PATH +  "/lib/tests/resources/config/test_crt.pem";
    std::string key = ROOT_PATH + "/lib/tests/resources/config/test_key.pem";

    const char* argv[] = { "test-command", "--server", server.data(), "--ca", ca.data(),
                           "--cert", cert.data(), "--key", key.data() };
    int argc= 9;

    Configuration::Instance().initialize(argc, const_cast<char**>(argv));
}

void resetTest() {
    Configuration::Instance().reset();
}

TEST_CASE("Configuration::setStartFunction", "[configuration]") {
    std::string server = "wss://test_server/";
    std::string ca = ROOT_PATH + "/lib/tests/resources/config/ca_crt.pem";
    std::string cert = ROOT_PATH +  "/lib/tests/resources/config/test_crt.pem";
    std::string key = ROOT_PATH + "/lib/tests/resources/config/test_key.pem";

    const char* argv[] = { "test-command", "--server", server.data(), "--ca", ca.data(),
                           "--cert", cert.data(), "--key", key.data() };
    int argc= 9;
    int test_val = 0;

    Configuration::Instance().setStartFunction([&test_val] (std::vector<std::string> arg) -> int {
        return 1;
    });

    Configuration::Instance().initialize(argc, const_cast<char**>(argv));

    REQUIRE(HorseWhisperer::Start() == 1);
}

TEST_CASE("Configuration::set", "[configuration]") {
    configureTest();

    SECTION("can set a known flag to a valid value") {
        REQUIRE_NOTHROW(Configuration::Instance()
                            .set<std::string>("ca", "value"));
    }

    SECTION("set a flag correctly") {
        Configuration::Instance().set<std::string>("ca", "value");
        REQUIRE(HorseWhisperer::GetFlag<std::string>("ca") == "value");
    }

    SECTION("throw when setting an unknown flag") {
        REQUIRE_THROWS_AS(Configuration::Instance()
                                .set<int>("tentacle_spawning_interval_s", 45),
                          configuration_entry_error);
    }

    SECTION("throw when setting a known flag to an invalid value") {
        HorseWhisperer::DefineGlobalFlag<int>("num_tentacles",
                                              "number of spawn tentacles",
                                              8,
                                              [](int v) -> bool{return v == 8;});

        // The only valid number of tentacles is 8
        REQUIRE_THROWS_AS(Configuration::Instance().set<int>("num_tentacles", 42),
                          configuration_entry_error);
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

        SECTION("throw a configuration_entry_error if the flag is unknown") {
            REQUIRE_THROWS_AS(
                Configuration::Instance().get<std::string>("dont_exist"),
                configuration_entry_error);
        }
    }

    SECTION("Configuration was initialized") {
        configureTest();

        SECTION("can get a defined flag") {
            REQUIRE_NOTHROW(Configuration::Instance().get<std::string>("ca"));
        }

        SECTION("return the default value if the flag was not set") {
            REQUIRE(Configuration::Instance().get<std::string>("spool-dir")
                    == DEFAULT_ACTION_RESULTS_DIR);
        }

        SECTION("return the correct value after the flag has been set") {
            Configuration::Instance().set<std::string>("spool-dir", "/fake/dir");
            REQUIRE(Configuration::Instance().get<std::string>("spool-dir")
                    == "/fake/dir");
        }

        SECTION("dont throw if the flag is unknown") {
            REQUIRE_NOTHROW(Configuration::Instance()
                                .get<std::string>("still_dont_exist"));
        }

        SECTION("return a default value if the flag is unknown") {
            REQUIRE(Configuration::Instance().get<std::string>("nada") == "");
        }

        resetTest();
    }
}

TEST_CASE("Configuration::validateAndNormalizeConfiguration", "[configuration]") {
    configureTest();

    SECTION("it fails when server is undefined") {
        Configuration::Instance().set<std::string>("server", "");
        REQUIRE_THROWS_AS(Configuration::Instance().validateAndNormalizeConfiguration(),
                          required_not_set_error);
    }

    SECTION("it fails when server is invlaid") {
        Configuration::Instance().set<std::string>("server", "ws://");
        REQUIRE_THROWS_AS(Configuration::Instance().validateAndNormalizeConfiguration(),
                          configuration_entry_error);
    }

    SECTION("it fails when ca is undefined") {
        Configuration::Instance().set<std::string>("ca", "");
        REQUIRE_THROWS_AS(Configuration::Instance().validateAndNormalizeConfiguration(),
                          required_not_set_error);
    }

    SECTION("it fails when ca file cannot be found") {
        Configuration::Instance().set<std::string>("ca", "/fake/file");
        REQUIRE_THROWS_AS(Configuration::Instance().validateAndNormalizeConfiguration(),
                          configuration_entry_error);
    }

    SECTION("it fails when cert is undefined") {
        Configuration::Instance().set<std::string>("cert", "");
        REQUIRE_THROWS_AS(Configuration::Instance().validateAndNormalizeConfiguration(),
                          required_not_set_error);
    }

    SECTION("it fails when cert file cannot be found") {
        Configuration::Instance().set<std::string>("cert", "/fake/file");
        REQUIRE_THROWS_AS(Configuration::Instance().validateAndNormalizeConfiguration(),
                          configuration_entry_error);
    }

    SECTION("it fails when key is undefined") {
        Configuration::Instance().set<std::string>("key", "");
        REQUIRE_THROWS_AS(Configuration::Instance().validateAndNormalizeConfiguration(),
                          required_not_set_error);
    }

    SECTION("it fails when key file cannot be found") {
        Configuration::Instance().set<std::string>("key", "/fake/file");
        REQUIRE_THROWS_AS(Configuration::Instance().validateAndNormalizeConfiguration(),
                          configuration_entry_error);
    }

    resetTest();
}

}  // namespace CthunAgent
