#pragma once

#include "./certs.hpp"
#include "root_path.hpp"

#include <pxp-agent/configuration.hpp>
#include <pxp-agent/pxp_connector.hpp>  // TEST_VIRTUAL, PXPConnector
#include <pxp-agent/action_request.hpp>
#include <pxp-agent/action_response.hpp>

namespace PXPAgent {

static const std::string TEST_BROKER_WS_URI { "wss://127.0.0.1:8090/pxp/" };
static const std::string TEST_FAILOVER_WS_URI { "wss://127.0.0.1:8091/pxp/" };
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
                                                  TEST_BROKER_WS_URI,
                                                  TEST_FAILOVER_WS_URI,
                                                  CA,
                                                  CERT,
                                                  KEY,
                                                  SPOOL,
                                                  "0d",  // don't purge!
                                                  "",    // modules config dir
                                                  "test_agent",
                                                  5000,  // connection timeout
                                                  10,    // association timeout
                                                  5,     // association ttl
                                                  5,     // general PCP ttl
                                                  2 };   // keepalive timeouts

static const std::string VALID_ENVELOPE_TXT {
    " { \"id\" : \"123456\","
    "   \"message_type\" : \"test_test_test\","
    "   \"expires\" : \"2015-06-26T22:57:09Z\","
    "   \"targets\" : [\"pcp://agent/test_agent\"],"
    "   \"sender\" : \"pcp://controller/test_controller\","
    "   \"destination_report\" : false"
    " }" };

#ifdef TEST_VIRTUAL

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

    virtual void sendPCPError(const std::string&,
                              const std::string&,
                              const std::vector<std::string>&);

    virtual void sendPXPError(const ActionRequest&,
                              const std::string&);

    virtual void sendPXPError(const ActionResponse&);

    virtual void sendBlockingResponse(const ActionResponse&,
                                      const ActionRequest&);

    virtual void sendNonBlockingResponse(const ActionResponse&);

    virtual void sendProvisionalResponse(const ActionRequest&);
};

#endif  // TEST_VIRTUAL

}  // namespace PXPAgent
