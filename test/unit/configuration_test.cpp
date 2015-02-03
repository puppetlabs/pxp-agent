#include "test/test.h"

#include "src/configuration.h"
#include "src/errors.h"

#include "horsewhisperer/horsewhisperer.h"

extern std::string ROOT_PATH;

void configure_test() {
    std::string server = "wss://test_server/";
    std::string ca = ROOT_PATH + "/test/resources/config/ca_crt.pem";
    std::string cert = ROOT_PATH +  "/test/resources/config/test_crt.pem";
    std::string key = ROOT_PATH + "/test/resources/config/test_key.pem";

    const char* argv[] = { "test-command", "--server", server.data(), "--ca", ca.data(),
                           "--cert", cert.data(), "--key", key.data() };
    int argc= 9;

    CthunAgent::Configuration::Instance().initialize(argc, const_cast<char**>(argv));
}

TEST_CASE("Configuration::setStartFunction", "[configuration]") {
    std::string server = "wss://test_server/";
    std::string ca = ROOT_PATH + "/test/resources/config/ca_crt.pem";
    std::string cert = ROOT_PATH +  "/test/resources/config/test_crt.pem";
    std::string key = ROOT_PATH + "/test/resources/config/test_key.pem";

    const char* argv[] = { "test-command", "--server", server.data(), "--ca", ca.data(),
                           "--cert", cert.data(), "--key", key.data() };
    int argc= 9;
    int test_val = 0;

    CthunAgent::Configuration::Instance().setStartFunction([&test_val] (std::vector<std::string> arg) -> int {
        return 1;
    });

    CthunAgent::Configuration::Instance().initialize(argc, const_cast<char**>(argv));

    REQUIRE(HorseWhisperer::Start() == 1);
}

TEST_CASE("Configuration::set", "[configuration]") {
    configure_test();

    SECTION("can set a known flag to a valid value") {
        REQUIRE_NOTHROW(CthunAgent::Configuration::Instance()
                            .set<std::string>("ca", "value"));
    }

    SECTION("set a flag correctly") {
        CthunAgent::Configuration::Instance().set<std::string>("ca", "value");
        REQUIRE(HorseWhisperer::GetFlag<std::string>("ca") == "value");
    }

    SECTION("throw when setting an unknown flag") {
        REQUIRE_THROWS_AS(CthunAgent::Configuration::Instance()
                            .set<int>("tentacle_spawning_interval_s", 45),
                          CthunAgent::configuration_entry_error);
    }

    SECTION("throw when setting a known flag to an invalid value") {
        HorseWhisperer::DefineGlobalFlag<int>("num_tentacles",
                                              "number of spawn tentacles",
                                              8,
                                              [](int v) -> bool{return v == 8;});

        REQUIRE_THROWS_AS(CthunAgent::Configuration::Instance()
                            .set<int>("num_tentacles", 42),
                          CthunAgent::configuration_entry_error);
        REQUIRE_NOTHROW(CthunAgent::Configuration::Instance()
                            .set<int>("num_tentacles", 8));
    }
}

TEST_CASE("Configuration::get", "[configuration]") {
    // TODO(ale): test Configuration before its initialization (we
    // would need to add a reset hook to the Configuration singleton)

    HorseWhisperer::SetFlag<std::string>("ca", "value");
    REQUIRE(CthunAgent::Configuration::Instance().get<std::string>("ca") == "value");
}

TEST_CASE("Configuration::validateConfiguration", "[configuration]") {
    configure_test();

    SECTION("it fails when server is undefined") {
        CthunAgent::Configuration::Instance().set<std::string>("server", "");
        REQUIRE_THROWS_AS(CthunAgent::Configuration::Instance().validateConfiguration(0),
                          CthunAgent::required_not_set_error);
    }

    SECTION("it fails when server is invlaid") {
        CthunAgent::Configuration::Instance().set<std::string>("server", "ws://");
        REQUIRE_THROWS_AS(CthunAgent::Configuration::Instance().validateConfiguration(0),
                          CthunAgent::required_not_set_error);
    }

    SECTION("it fails when ca is undefined") {
        CthunAgent::Configuration::Instance().set<std::string>("ca", "");
        REQUIRE_THROWS_AS(CthunAgent::Configuration::Instance().validateConfiguration(0),
                          CthunAgent::required_not_set_error);
    }

    SECTION("it fails when ca file cannot be found") {
        CthunAgent::Configuration::Instance().set<std::string>("ca", "/fake/file");
        REQUIRE_THROWS_AS(CthunAgent::Configuration::Instance().validateConfiguration(0),
                          CthunAgent::required_not_set_error);
    }

    SECTION("it fails when cert is undefined") {
        CthunAgent::Configuration::Instance().set<std::string>("cert", "");
        REQUIRE_THROWS_AS(CthunAgent::Configuration::Instance().validateConfiguration(0),
                          CthunAgent::required_not_set_error);
    }

    SECTION("it fails when cert file cannot be found") {
        CthunAgent::Configuration::Instance().set<std::string>("cert", "/fake/file");
        REQUIRE_THROWS_AS(CthunAgent::Configuration::Instance().validateConfiguration(0),
                          CthunAgent::required_not_set_error);
    }

    SECTION("it fails when key is undefined") {
        CthunAgent::Configuration::Instance().set<std::string>("key", "");
        REQUIRE_THROWS_AS(CthunAgent::Configuration::Instance().validateConfiguration(0),
                          CthunAgent::required_not_set_error);
    }

    SECTION("it fails when key file cannot be found") {
        CthunAgent::Configuration::Instance().set<std::string>("key", "/fake/file");
        REQUIRE_THROWS_AS(CthunAgent::Configuration::Instance().validateConfiguration(0),
                          CthunAgent::required_not_set_error);
    }
}
