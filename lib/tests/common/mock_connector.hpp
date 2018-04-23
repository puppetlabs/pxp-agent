#pragma once

#include "./certs.hpp"
#include "root_path.hpp"

#include <pxp-agent/configuration.hpp>
#include <pxp-agent/pxp_connector.hpp>  // PXPConnector
#include <pxp-agent/action_request.hpp>
#include <pxp-agent/action_response.hpp>

#include <atomic>
#include <vector>
#include <string>

namespace PXPAgent {

static const std::vector<std::string> TEST_BROKER_WS_URIS { "wss://127.0.0.1:8090/pxp/", "wss://127.0.0.1:8091/pxp/" };
static const std::string PCP_VERSION { "1" };
static const std::string CA { getCaPath() };
static const std::string CERT { getCertPath() };
static const std::string KEY { getKeyPath() };
static const std::string MODULES { PXP_AGENT_ROOT_PATH
            + std::string { "/lib/tests/resources/modules" } };
static const std::string VALID_MODULES_CONFIG { PXP_AGENT_ROOT_PATH
            + std::string { "/lib/tests/resources/modules_config_valid" } };
static const std::string BAD_FORMAT_MODULES_CONFIG { PXP_AGENT_ROOT_PATH
            + std::string { "/lib/tests/resources/modules_config_bad_format" } };
static const std::string BROKEN_MODULES_CONFIG { PXP_AGENT_ROOT_PATH
            + std::string { "/lib/tests/resources/modules_config_broken" } };
static const std::string SPOOL { PXP_AGENT_ROOT_PATH
            + std::string { "/lib/tests/resources/tmp" } };

static Configuration::Agent AGENT_CONFIGURATION { MODULES,
                                                  TEST_BROKER_WS_URIS,
                                                  std::vector<std::string> {},  // master uris
                                                  PCP_VERSION,
                                                  CA,
                                                  CERT,
                                                  KEY,
                                                  SPOOL,
                                                  "0d",  // don't purge spool!
                                                  "",    // modules config dir
                                                  "",    // task cache dir
                                                  "0d",  // don't purge task cache!
                                                  "test_agent",
                                                  5000,  // connection timeout
                                                  10,    // association timeout
                                                  5,     // association ttl
                                                  5,     // general PCP ttl
                                                  2,     // keepalive timeouts
                                                  15,    // ping interval
                                                  30,    // task download connection timeout
                                                  120};  // task download timeout

static const std::string VALID_ENVELOPE_TXT {
    " { \"id\" : \"123456\","
    "   \"message_type\" : \"test_test_test\","
    "   \"expires\" : \"2015-06-26T22:57:09Z\","
    "   \"targets\" : [\"pcp://agent/test_agent\"],"
    "   \"sender\" : \"pcp://controller/test_controller\","
    "   \"destination_report\" : false"
    " }" };

class MockConnector : public PXPConnector {
  public:
    struct pcpError_msg : public std::exception {
        const char* what() const noexcept { return "PCP error"; } };
    struct pxpError_msg : public std::exception {
        const char* what() const noexcept { return "PXP error"; } };

    std::atomic<bool> sent_provisional_response;
    std::atomic<bool> sent_non_blocking_response;
    std::atomic<bool> sent_blocking_response;

    MockConnector();

    void sendPCPError(const std::string&,
                      const std::string&,
                      const std::vector<std::string>&) override;

    void sendPXPError(const ActionRequest&,
                      const std::string&) override;

    void sendPXPError(const ActionResponse&) override;

    void sendBlockingResponse(const ActionResponse&,
                              const ActionRequest&) override;

    void sendStatusResponse(const ActionResponse& response,
                            const ActionRequest& request) override;

    void sendNonBlockingResponse(const ActionResponse&) override;

    void sendProvisionalResponse(const ActionRequest&) override;

    void connect(int max_connect_attempts = 0) override;

    void monitorConnection(uint32_t max_connect_attempts = 0,
                           uint32_t connection_check_interval_s = 15) override;

    void registerMessageCallback(const PCPClient::Schema& schema,
                                 MessageCallback callback) override;
};

}  // namespace PXPAgent
