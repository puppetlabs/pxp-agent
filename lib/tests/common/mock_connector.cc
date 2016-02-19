#include "mock_connector.hpp"

namespace PXPAgent {

#ifdef TEST_VIRTUAL

MockConnector::MockConnector()
        : PXPConnector { AGENT_CONFIGURATION },
          sent_provisional_response { false },
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

void MockConnector::sendNonBlockingResponse(const ActionResponse&)
{
    sent_non_blocking_response = true;
}

void MockConnector::sendProvisionalResponse(const ActionRequest&)
{
    sent_provisional_response = true;
}

#endif  // TEST_VIRTUAL

}  // namespace PXPAgent
