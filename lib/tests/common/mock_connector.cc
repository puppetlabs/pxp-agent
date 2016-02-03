#include "mock_connector.hpp"

namespace PXPAgent {

namespace lth_jc = leatherman::json_container;

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

void MockConnector::sendBlockingResponse(const ActionRequest&,
                                         const lth_jc::JsonContainer&)
{
    sent_blocking_response = true;
}

void MockConnector::sendNonBlockingResponse(const ActionRequest&,
                                            const lth_jc::JsonContainer&,
                                            const std::string&)
{
    sent_non_blocking_response = true;
}

void MockConnector::sendProvisionalResponse(const ActionRequest&)
{
    sent_provisional_response = true;
}

#endif  // TEST_VIRTUAL

}  // namespace PXPAgent
