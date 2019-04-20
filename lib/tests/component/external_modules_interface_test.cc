#include "../common/certs.hpp"
#include "../common/mock_connector.hpp"

#include "root_path.hpp"

#include <pxp-agent/external_module.hpp>
#include <pxp-agent/request_processor.hpp>
#include <pxp-agent/configuration.hpp>
#include <pxp-agent/results_storage.hpp>

#include <leatherman/json_container/json_container.hpp>
#include <leatherman/util/timer.hpp>

#include <cpp-pcp-client/util/thread.hpp>   // this_thread::sleep_for
#include <cpp-pcp-client/util/chrono.hpp>

#include <boost/filesystem.hpp>
#include <boost/filesystem/operations.hpp>

#include <catch.hpp>

using namespace PXPAgent;

namespace lth_jc = leatherman::json_container;
namespace lth_util = leatherman::util;
namespace pcp_util = PCPClient::Util;
namespace fs = boost::filesystem;

static const std::string REVERSE_VALID_MODULE_NAME { "reverse_valid" };

TEST_CASE("Valid External Module Configuration", "[component]") {
    AGENT_CONFIGURATION.modules_config_dir = VALID_MODULES_CONFIG;
    auto c_ptr = std::make_shared<MockConnector>();
    RequestProcessor r_p { c_ptr, AGENT_CONFIGURATION };

    SECTION("retrieves the configuration from file if valid JSON format") {
        REQUIRE(r_p.hasModuleConfig(REVERSE_VALID_MODULE_NAME));
    }

    SECTION("does load the module when the configuration is valid") {
        REQUIRE(r_p.hasModuleConfig(REVERSE_VALID_MODULE_NAME));
        REQUIRE(r_p.hasModule(REVERSE_VALID_MODULE_NAME));
    }
}

TEST_CASE("Badly formatted External Module Configuration", "[component]") {
    AGENT_CONFIGURATION.modules_config_dir = BAD_FORMAT_MODULES_CONFIG;
    auto c_ptr = std::make_shared<MockConnector>();
    RequestProcessor r_p { c_ptr, AGENT_CONFIGURATION };

    SECTION("stores a null JSON value as the configuration, if it's in an "
            "invalid JSON format") {
        REQUIRE(r_p.getModuleConfig(REVERSE_VALID_MODULE_NAME) == "null");
    }

    SECTION("does not load the module when the configuration is in "
            "bad JSON format") {
        REQUIRE(r_p.getModuleConfig(REVERSE_VALID_MODULE_NAME) == "null");
        REQUIRE_FALSE(r_p.hasModule(REVERSE_VALID_MODULE_NAME));
    }
}

TEST_CASE("Invalid (by metadata) External Module Configuration", "[component]") {
    SECTION("does not load the module when the configuration is invalid") {
        AGENT_CONFIGURATION.modules_config_dir = BROKEN_MODULES_CONFIG;
        auto c_ptr = std::make_shared<MockConnector>();
        RequestProcessor r_p { c_ptr, AGENT_CONFIGURATION };
        REQUIRE(r_p.hasModuleConfig(REVERSE_VALID_MODULE_NAME));
        REQUIRE_FALSE(r_p.hasModule(REVERSE_VALID_MODULE_NAME));
    }
}

static ResultsStorage test_storage { SPOOL, "0d" };

void wait_for_module(ResultsStorage& storage, const std::string& t_id)
{
    lth_util::Timer t {};
    while (!storage.outputIsReady(t_id)) {
        if (t.elapsed_seconds() > 10)
            FAIL("External Module ran out of time");
        pcp_util::this_thread::sleep_for(
            pcp_util::chrono::milliseconds(10));
    }
}

TEST_CASE("Process correctly requests for external modules", "[component]") {
    if (!fs::exists(SPOOL) && !fs::create_directories(SPOOL)) {
        FAIL("Failed to create the results directory");
    }

    AGENT_CONFIGURATION.modules_config_dir = "";
    auto c_ptr = std::make_shared<MockConnector>();
    RequestProcessor r_p { c_ptr, AGENT_CONFIGURATION };
    lth_jc::JsonContainer envelope { VALID_ENVELOPE_TXT };
    std::vector<lth_jc::JsonContainer> debug {};
    lth_jc::JsonContainer data {};

    lth_jc::JsonContainer dummy_metadata {};
    dummy_metadata.set<std::string>("requester", "foo");
    dummy_metadata.set<std::string>("module", "reverse_valid");
    dummy_metadata.set<std::string>("action", "string");
    dummy_metadata.set<std::string>("request_params", "abc");
    dummy_metadata.set<std::string>("transaction_id", "0");
    dummy_metadata.set<std::string>("request_id", "0");
    dummy_metadata.set<bool>("notify_outcome", false);
    dummy_metadata.set<std::string>("start", "2017-01-27T23:16:12.459948Z");
    dummy_metadata.set<std::string>("status", "failed");

    SECTION("correctly process blocking requests") {
        SECTION("send a blocking response when the requested action succeeds") {
            REQUIRE(!c_ptr->sent_blocking_response);
            data.set<std::string>("transaction_id", "32");
            data.set<std::string>("module", "reverse_valid");
            data.set<std::string>("action", "string");
            lth_jc::JsonContainer params {};
            params.set<std::string>("argument", "was");
            data.set<lth_jc::JsonContainer>("params", params);
            const PCPClient::ParsedChunks p_c { envelope, data, debug, 0 };

            REQUIRE_NOTHROW(r_p.processRequest(RequestType::Blocking, p_c));
            REQUIRE(c_ptr->sent_blocking_response);
        }

        SECTION("send a PXP error in case of action failure") {
            data.set<std::string>("transaction_id", "64");
            data.set<std::string>("module", "failures_test");
            data.set<std::string>("action", "broken_action");
            lth_jc::JsonContainer params {};
            params.set<std::string>("argument", "bikini");
            data.set<lth_jc::JsonContainer>("params", params);
            const PCPClient::ParsedChunks p_c { envelope, data, debug, 0 };

            REQUIRE_THROWS_AS(r_p.processRequest(RequestType::Blocking, p_c),
                              MockConnector::pxpError_msg&);
        }
    }

    SECTION("correctly process non-blocking requests") {
        SECTION("send a provisional response when the requested action starts "
                "successfully") {
            REQUIRE_FALSE(c_ptr->sent_provisional_response);

            const std::string t_id { "128" };
            data.set<std::string>("transaction_id", t_id);
            data.set<std::string>("module", "reverse_valid");
            data.set<bool>("notify_outcome", false);
            data.set<std::string>("action", "string");
            lth_jc::JsonContainer params {};
            params.set<std::string>("argument", "lemon");
            data.set<lth_jc::JsonContainer>("params", params);
            const PCPClient::ParsedChunks p_c { envelope, data, debug, 0 };

            REQUIRE_NOTHROW(r_p.processRequest(RequestType::NonBlocking, p_c));

            wait_for_module(test_storage, t_id);

            REQUIRE(c_ptr->sent_provisional_response);

            // Wait to let the execution thread process the output
            // (there's a OUTPUT_DELAY_MS pause) and send the
            // non-blocking response
            pcp_util::this_thread::sleep_for(
                pcp_util::chrono::milliseconds(100 + ExternalModule::OUTPUT_DELAY_MS));

            REQUIRE_FALSE(c_ptr->sent_non_blocking_response);
        }

        SECTION("send a provisional response when a duplicate request for an active"
                " action is received") {
            REQUIRE_FALSE(c_ptr->sent_provisional_response);

            const std::string t_id { "192" };
            data.set<std::string>("transaction_id", t_id);
            data.set<std::string>("module", "reverse_valid");
            data.set<bool>("notify_outcome", false);
            data.set<std::string>("action", "string");
            lth_jc::JsonContainer params {};
            params.set<std::string>("argument", "lemon");
            data.set<lth_jc::JsonContainer>("params", params);
            const PCPClient::ParsedChunks p_c { envelope, data, debug, 0 };

            REQUIRE_NOTHROW(r_p.processRequest(RequestType::NonBlocking, p_c));
            REQUIRE(c_ptr->sent_provisional_response);

            wait_for_module(test_storage, t_id);

            // Wait to let the execution thread process the output
            // (there's a OUTPUT_DELAY_MS pause) and update metadata
            pcp_util::this_thread::sleep_for(
                pcp_util::chrono::milliseconds(100 + ExternalModule::OUTPUT_DELAY_MS));

            // Update metadata file
            test_storage.updateMetadataFile(t_id, dummy_metadata);

            // Resend the same request
            c_ptr->sent_provisional_response = false;
            REQUIRE_NOTHROW(r_p.processRequest(RequestType::NonBlocking, p_c));
            REQUIRE(c_ptr->sent_provisional_response);

            // Verify metadata file wasn't updated, i.e. no action was run. "request_params" should be emptied to
            // prevent writing sensitive metadata to disk.
            dummy_metadata.set<std::string>("request_params", "{}");
            REQUIRE(dummy_metadata.toString().compare(test_storage.getActionMetadata(t_id).toString()) == 0);
        }

        SECTION("send a provisional response when a duplicate request for a forgotten"
                " action is received") {
            REQUIRE_FALSE(c_ptr->sent_provisional_response);

            const std::string t_id { "256" };
            data.set<std::string>("transaction_id", t_id);
            data.set<std::string>("module", "reverse_valid");
            data.set<bool>("notify_outcome", false);
            data.set<std::string>("action", "string");
            lth_jc::JsonContainer params {};
            params.set<std::string>("argument", "lemon");
            data.set<lth_jc::JsonContainer>("params", params);
            const PCPClient::ParsedChunks p_c { envelope, data, debug, 0 };

            REQUIRE_NOTHROW(r_p.processRequest(RequestType::NonBlocking, p_c));
            REQUIRE(c_ptr->sent_provisional_response);

            wait_for_module(test_storage, t_id);

            // Wait to let the execution thread process the output
            // (there's a OUTPUT_DELAY_MS pause) and update metadata
            pcp_util::this_thread::sleep_for(
                pcp_util::chrono::milliseconds(100 + ExternalModule::OUTPUT_DELAY_MS));

            // Update metadata file
            test_storage.updateMetadataFile(t_id, dummy_metadata);

            // Create a new request processor and send the same request
            RequestProcessor r_p2 { c_ptr, AGENT_CONFIGURATION };
            c_ptr->sent_provisional_response = false;
            REQUIRE_NOTHROW(r_p2.processRequest(RequestType::NonBlocking, p_c));
            REQUIRE(c_ptr->sent_provisional_response);

            // Verify metadata file wasn't updated, i.e. no action was run. "request_params" should be emptied to
            // prevent writing sensitive metadata to disk.
            dummy_metadata.set<std::string>("request_params", "{}");
            REQUIRE(dummy_metadata.toString().compare(test_storage.getActionMetadata(t_id).toString()) == 0);
        }

        SECTION("send a non-blocking response when the requested action succeeds") {
            REQUIRE_FALSE(c_ptr->sent_provisional_response);
            REQUIRE_FALSE(c_ptr->sent_non_blocking_response);

            const std::string t_id { "512" };
            data.set<std::string>("transaction_id", t_id);
            data.set<std::string>("module", "reverse_valid");
            data.set<bool>("notify_outcome", true);
            data.set<std::string>("action", "string");
            lth_jc::JsonContainer params {};
            params.set<std::string>("argument", "kondogbia");
            data.set<lth_jc::JsonContainer>("params", params);
            const PCPClient::ParsedChunks p_c { envelope, data, debug, 0 };

            REQUIRE_NOTHROW(r_p.processRequest(RequestType::NonBlocking, p_c));

            wait_for_module(test_storage, t_id);

            REQUIRE(c_ptr->sent_provisional_response);

            // Wait to let the execution thread process the output
            // (there's a OUTPUT_DELAY_MS pause) and send the
            // non-blocking response
            pcp_util::this_thread::sleep_for(
                pcp_util::chrono::milliseconds(100 + ExternalModule::OUTPUT_DELAY_MS));

            REQUIRE(c_ptr->sent_non_blocking_response);
        }
    }

    fs::remove_all(SPOOL);
}
