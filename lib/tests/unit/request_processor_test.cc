#include "../common/certs.hpp"
#include "../common/mock_connector.hpp"
#include "root_path.hpp"

#include <pxp-agent/request_processor.hpp>
#include <pxp-agent/configuration.hpp>

#include <leatherman/json_container/json_container.hpp>

#include <catch.hpp>

#include <boost/filesystem.hpp>
#include <boost/filesystem/operations.hpp>

#include <memory>
#include <exception>
#include <unistd.h>
#include <atomic>

using namespace PXPAgent;

namespace lth_jc = leatherman::json_container;
namespace pcp_util = PCPClient::Util;
namespace fs = boost::filesystem;

TEST_CASE("RequestProcessor::RequestProcessor", "[agent]") {
    auto c_ptr = std::make_shared<MockConnector>();

    SECTION("successfully instantiates with valid arguments") {
        REQUIRE_NOTHROW(RequestProcessor(c_ptr, AGENT_CONFIGURATION));
    };

    SECTION("instantiates if the specified modules directory does not exist") {
        Configuration::Agent a_c  = AGENT_CONFIGURATION;
        a_c.modules_dir += "/fake_dir";

        REQUIRE_NOTHROW(RequestProcessor(c_ptr, a_c));
    };

    fs::remove_all(SPOOL);
}

TEST_CASE("RequestProcessor::hasModule", "[agent]") {
    AGENT_CONFIGURATION.modules_config_dir = VALID_MODULES_CONFIG;
    auto c_ptr = std::make_shared<MockConnector>();
    RequestProcessor r_p { c_ptr, AGENT_CONFIGURATION };

    SECTION("returns true if the requested module was loaded") {
        REQUIRE(r_p.hasModule("reverse_valid"));
    }

    SECTION("returns false if the requested module was not loaded") {
        REQUIRE_FALSE(r_p.hasModule("this_module_here_does_not_exist"));
    }
}

TEST_CASE("RequestProcessor::hasModuleConfig", "[agent]") {
    SECTION("valid module configuration") {
        AGENT_CONFIGURATION.modules_config_dir = VALID_MODULES_CONFIG;
        auto c_ptr = std::make_shared<MockConnector>();
        RequestProcessor r_p { c_ptr, AGENT_CONFIGURATION };

        REQUIRE(r_p.hasModuleConfig("reverse_valid"));
    }

    SECTION("badly formatted module configuration") {
        AGENT_CONFIGURATION.modules_config_dir = BAD_FORMAT_MODULES_CONFIG;
        auto c_ptr = std::make_shared<MockConnector>();
        RequestProcessor r_p { c_ptr, AGENT_CONFIGURATION };

        REQUIRE(r_p.hasModuleConfig("reverse_valid"));
    }

    SECTION("non existent module configuration") {
        AGENT_CONFIGURATION.modules_config_dir = VALID_MODULES_CONFIG;
        auto c_ptr = std::make_shared<MockConnector>();
        RequestProcessor r_p { c_ptr, AGENT_CONFIGURATION };

        REQUIRE_FALSE(r_p.hasModuleConfig("this_module_here_does_not_exist"));
    }
}

TEST_CASE("RequestProcessor::getModuleConfig", "[agent]") {
    SECTION("valid module configuration") {
        AGENT_CONFIGURATION.modules_config_dir = VALID_MODULES_CONFIG;
        auto c_ptr = std::make_shared<MockConnector>();
        RequestProcessor r_p { c_ptr, AGENT_CONFIGURATION };
        auto json_txt = r_p.getModuleConfig("reverse_valid");

        try {
            lth_jc::JsonContainer json { json_txt };
            REQUIRE(json.type() == lth_jc::Object);
        } catch (const lth_jc::data_error&) {
            FAIL("expected configuration in a valid JSON format");
        }
    }

    SECTION("badly formatted module configuration") {
        AGENT_CONFIGURATION.modules_config_dir = BAD_FORMAT_MODULES_CONFIG;
        auto c_ptr = std::make_shared<MockConnector>();
        RequestProcessor r_p { c_ptr, AGENT_CONFIGURATION };

        REQUIRE(r_p.getModuleConfig("reverse_valid") ==  "null");
    }

    SECTION("non existent module configuration") {
        AGENT_CONFIGURATION.modules_config_dir = VALID_MODULES_CONFIG;
        auto c_ptr = std::make_shared<MockConnector>();
        RequestProcessor r_p { c_ptr, AGENT_CONFIGURATION };

        REQUIRE_THROWS_AS(r_p.getModuleConfig("this_module_here_does_not_exist"),
                          RequestProcessor::Error);
    }
}

// NOTE(ale): the following tests require the MockConnector since they
// trigger WebSocket functions.

TEST_CASE("RequestProcessor::processRequest", "[agent]") {
    auto c_ptr = std::make_shared<MockConnector>();
    RequestProcessor r_p { c_ptr, AGENT_CONFIGURATION };
    lth_jc::JsonContainer envelope { VALID_ENVELOPE_TXT };
    std::vector<lth_jc::JsonContainer> debug {};

    SECTION("reply with a PCP error if request has bad format") {
        const PCPClient::ParsedChunks p_c { envelope, false, debug, 0 };

        REQUIRE_THROWS_AS(r_p.processRequest(RequestType::Blocking, p_c),
                          MockConnector::pcpError_msg);
    }

    lth_jc::JsonContainer data {};
    data.set<std::string>("transaction_id", "42");

    SECTION("reply with a PXP error if request has invalid content") {
        SECTION("uknown module") {
            data.set<std::string>("module", "foo");
            data.set<std::string>("action", "bar");
            const PCPClient::ParsedChunks p_c { envelope, data, debug, 0 };

            REQUIRE_THROWS_AS(r_p.processRequest(RequestType::Blocking, p_c),
                              MockConnector::pxpError_msg);
        }

        SECTION("uknown action") {
            data.set<std::string>("module", "reverse");
            data.set<std::string>("action", "bar");
            const PCPClient::ParsedChunks p_c { envelope, data, debug, 0 };

            REQUIRE_THROWS_AS(r_p.processRequest(RequestType::Blocking, p_c),
                              MockConnector::pxpError_msg);
        }
    }

    fs::remove_all(SPOOL);
}
