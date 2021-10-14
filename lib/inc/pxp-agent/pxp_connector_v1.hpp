#pragma once

#include <pxp-agent/pxp_connector.hpp>
#include <pxp-agent/action_request.hpp>
#include <pxp-agent/action_response.hpp>
#include <pxp-agent/configuration.hpp>

#include <cpp-pcp-client/connector/v1/connector.hpp>

#include <cstdint>

namespace PXPAgent {

class PXPConnectorV1 : public PCPClient::v1::Connector, public PXPConnector {
  public:
    PXPConnectorV1(const Configuration::Agent& agent_configuration, boost::nowide::ofstream* logstream);

    void sendPCPError(const std::string& request_id,
                      const std::string& description,
                      const std::vector<std::string>& endpoints) override;

    void sendProvisionalResponse(const ActionRequest& request) override;

    void sendPXPError(const ActionRequest& request,
                      const std::string& description) override;

    // Asserts that the ActionResponse arg has all needed entries.
    void sendPXPError(const ActionResponse& response) override;

    // Asserts that the ActionResponse arg has all needed entries.
    void sendBlockingResponse(const ActionResponse& response,
                              const ActionRequest& request) override;

    // Asserts that the ActionResponse arg has all needed entries.
    void sendStatusResponse(const ActionResponse& response,
                            const ActionRequest& request) override;

    // Asserts that the ActionResponse arg has all needed entries.
    void sendNonBlockingResponse(const ActionResponse& response) override;

    void connect(int max_connect_attempts = 0) override;

    void monitorConnection(uint32_t max_connect_attempts = 0,
                           uint32_t connection_check_interval_s = 15) override;

    void registerMessageCallback(const PCPClient::Schema& schema,
                                 MessageCallback callback) override;

  private:
    uint32_t pcp_message_ttl_s;

    void sendBlockingResponse_(const ActionResponse::ResponseType& response_type,
                               const ActionResponse& response,
                               const ActionRequest& request);
};

}  // namespace PXPAgent
