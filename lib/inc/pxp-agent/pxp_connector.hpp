#pragma once

#include <vector>
#include <string>
#include <functional>

namespace PCPClient {
struct ParsedChunks;
class Schema;
}  // namespace PCPClient

namespace PXPAgent {

class ActionRequest;
class ActionResponse;

using MessageCallback = std::function<void(const PCPClient::ParsedChunks& parsed_chunks)>;

// In case of failure, the send() methods will only log the failure;
// no exception will be propagated.
class PXPConnector {
  public:
    virtual void sendPCPError(const std::string& request_id,
                              const std::string& description,
                              const std::vector<std::string>& endpoints) = 0;

    virtual void sendPXPError(const ActionRequest& request,
                              const std::string& description) = 0;

    // Asserts that the ActionResponse arg has all needed entries.
    virtual void sendPXPError(const ActionResponse& response) = 0;

    // Asserts that the ActionResponse arg has all needed entries.
    virtual void sendBlockingResponse(const ActionResponse& response,
                                      const ActionRequest& request) = 0;

    // Asserts that the ActionResponse arg has all needed entries.
    virtual void sendStatusResponse(const ActionResponse& response,
                                    const ActionRequest& request) = 0;

    // Asserts that the ActionResponse arg has all needed entries.
    virtual void sendNonBlockingResponse(const ActionResponse& response) = 0;

    virtual void sendProvisionalResponse(const ActionRequest& request) = 0;

    virtual void connect(int max_connect_attempts = 0) = 0;

    virtual void monitorConnection(uint32_t max_connect_attempts = 0,
                                   uint32_t connection_check_interval_s = 15) = 0;

    virtual void registerMessageCallback(const PCPClient::Schema& schema,
                                         MessageCallback callback) = 0;
};

}  // namespace PXPAgent
