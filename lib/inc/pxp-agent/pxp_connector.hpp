#ifndef SRC_AGENT_PXP_CONNECTOR_HPP_
#define SRC_AGENT_PXP_CONNECTOR_HPP_

#ifdef TEST_VIRTUAL
#define TEST_VIRTUAL_SPECIFIER virtual
#else
#define TEST_VIRTUAL_SPECIFIER
#endif

#include <pxp-agent/action_request.hpp>
#include <pxp-agent/action_response.hpp>
#include <pxp-agent/configuration.hpp>

#include <cpp-pcp-client/connector/connector.hpp>

#include <leatherman/json_container/json_container.hpp>

#include <cassert>
#include <memory>
#include <vector>

namespace PXPAgent {

// In case of failure, the send() methods will only log the failure;
// no exception will be propagated.
class PXPConnector : public PCPClient::Connector {
  public:
    PXPConnector(const Configuration::Agent& agent_configuration);

    TEST_VIRTUAL_SPECIFIER void sendPCPError(const std::string& request_id,
                                             const std::string& description,
                                             const std::vector<std::string>& endpoints);

    TEST_VIRTUAL_SPECIFIER void sendProvisionalResponse(const ActionRequest& request);

    TEST_VIRTUAL_SPECIFIER void sendPXPError(const ActionRequest& request,
                                             const std::string& description);

    // Asserts that the ActionResponse arg has all needed entries.
    TEST_VIRTUAL_SPECIFIER void sendPXPError(const ActionResponse& response);

    // Asserts that the ActionResponse arg has all needed entries.
    TEST_VIRTUAL_SPECIFIER void sendBlockingResponse(const ActionResponse& response,
                                                     const ActionRequest& request);

    // Asserts that the ActionResponse arg has all needed entries.
    TEST_VIRTUAL_SPECIFIER void sendStatusResponse(const ActionResponse& response,
                                                   const ActionRequest& request);

    // Asserts that the ActionResponse arg has all needed entries.
    TEST_VIRTUAL_SPECIFIER void sendNonBlockingResponse(const ActionResponse& response);

  private:
    void sendBlockingResponse_(const ActionResponse::ResponseType& response_type,
                               const ActionResponse& response,
                               const ActionRequest& request);
};

}  // namespace PXPAgent

#endif  // SRC_AGENT_PXP_CONNECTOR_HPP_
