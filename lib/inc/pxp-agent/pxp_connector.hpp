#ifndef SRC_AGENT_PXP_CONNECTOR_HPP_
#define SRC_AGENT_PXP_CONNECTOR_HPP_

#ifdef TEST_VIRTUAL
#define TEST_VIRTUAL_SPECIFIER virtual
#else
#define TEST_VIRTUAL_SPECIFIER
#endif

#include <pxp-agent/action_request.hpp>
#include <pxp-agent/configuration.hpp>

#include <cpp-pcp-client/connector/connector.hpp>

#include <leatherman/json_container/json_container.hpp>

#include <cassert>
#include <memory>

namespace PXPAgent {

namespace lth_jc = leatherman::json_container;

class PXPConnector : public PCPClient::Connector {
  public:
    PXPConnector(const Configuration::Agent& agent_configuration);

    TEST_VIRTUAL_SPECIFIER void sendPCPError(
                    const std::string& request_id,
                    const std::string& description,
                    const std::vector<std::string>& endpoints);

    TEST_VIRTUAL_SPECIFIER void sendPXPError(
                    const ActionRequest& request,
                    const std::string& description);

    TEST_VIRTUAL_SPECIFIER void sendBlockingResponse(
                    const ActionRequest& request,
                    const leatherman::json_container::JsonContainer& results);

    TEST_VIRTUAL_SPECIFIER void sendNonBlockingResponse(
                    const ActionRequest& request,
                    const leatherman::json_container::JsonContainer& results,
                    const std::string& job_id);

    TEST_VIRTUAL_SPECIFIER void sendProvisionalResponse(
                    const ActionRequest& request);
};

}  // namespace PXPAgent

#endif  // SRC_AGENT_PXP_CONNECTOR_HPP_
