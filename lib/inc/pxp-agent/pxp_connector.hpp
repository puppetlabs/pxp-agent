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

#include <leatherman/json_container/json_container.hpp>
#include <leatherman/json_container/schema.hpp>

#include <cassert>
#include <memory>
#include <vector>
#include <cstdint>
#include <string>
#include <functional>

namespace PCPClient {
    class Connector;
    struct ParsedChunks;
}

namespace PXPAgent {

// In case of failure, the send() methods will only log the failure;
// no exception will be propagated.
class PXPConnector {
  public:
    using MessageCallback = std::function<void(std::string id,
                                               std::string sender,
                                               leatherman::json_container::JsonContainer data,
                                               std::vector<leatherman::json_container::JsonContainer> debug)>;

    PXPConnector(const Configuration::Agent& agent_configuration);

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

    // Establishes a connection
    TEST_VIRTUAL_SPECIFIER void connect();

    TEST_VIRTUAL_SPECIFIER void registerMessageCallback(const leatherman::json_container::Schema& schema,
                                                        MessageCallback callback);

  private:
    uint32_t pcp_message_ttl_s;
    std::shared_ptr<PCPClient::Connector> pcp_connector_;

    void sendBlockingResponse_(const ActionResponse::ResponseType& response_type,
                               const ActionResponse& response,
                               const ActionRequest& request);

    void sendPCPError_(const std::string& request_id,
                       const std::string& description,
                       const std::vector<std::string>& endpoints);

    bool validateFormat_(const std::string& id,
                         const std::string& sender,
                         const PCPClient::ParsedChunks& parsed_chunks);
};

}  // namespace PXPAgent

#endif  // SRC_AGENT_PXP_CONNECTOR_HPP_
