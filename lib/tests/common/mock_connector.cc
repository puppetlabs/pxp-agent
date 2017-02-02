#include "mock_connector.hpp"

namespace PXPAgent {

MockConnector::MockConnector()
        : sent_provisional_response { false },
          sent_non_blocking_response { false },
          sent_blocking_response { false }
{
}

void MockConnector::sendPCPError(const std::string&,
                                 const std::string&,
                                 const std::vector<std::string>&)
{
    throw MockConnector::pcpError_msg {};
}

void MockConnector::sendPXPError(const ActionRequest&,
                                 const std::string&)
{
    throw MockConnector::pxpError_msg {};
}

void MockConnector::sendPXPError(const ActionResponse&)
{
    throw MockConnector::pxpError_msg {};
}

void MockConnector::sendBlockingResponse(const ActionResponse&,
                                         const ActionRequest&)
{
    sent_blocking_response = true;
}

void MockConnector::sendStatusResponse(const ActionResponse& response,
                                       const ActionRequest& request)
{
    throw MockConnector::pxpError_msg {};
}

void MockConnector::sendNonBlockingResponse(const ActionResponse&)
{
    sent_non_blocking_response = true;
}

void MockConnector::sendProvisionalResponse(const ActionRequest&)
{
    sent_provisional_response = true;
}

void MockConnector::connect(int max_connect_attempts)
{
    throw MockConnector::pxpError_msg {};
}

void MockConnector::monitorConnection(uint32_t max_connect_attempts,
                                      uint32_t connection_check_interval_s)
{
    throw MockConnector::pxpError_msg {};
}

void MockConnector::registerMessageCallback(const PCPClient::Schema& schema,
                                            MessageCallback callback)
{
    throw MockConnector::pxpError_msg {};
}

}  // namespace PXPAgent
