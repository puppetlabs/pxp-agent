#ifndef SRC_AGENT_CTHUN_CONNECTOR_H_
#define SRC_AGENT_CTHUN_CONNECTOR_H_

#ifdef TEST_VIRTUAL
#define TEST_VIRTUAL_SPECIFIER virtual
#else
#define TEST_VIRTUAL_SPECIFIER
#endif

#include <cthun-agent/action_request.hpp>

#include <cthun-client/connector/connector.hpp>

#include <leatherman/json_container/json_container.hpp>

#include <cassert>
#include <memory>

namespace CthunAgent {

class CthunConnector : public CthunClient::Connector {
  public:
    CthunConnector(const std::string& server_url,
                   const std::string& client_type,
                   const std::string& ca_crt_path,
                   const std::string& client_crt_path,
                   const std::string& client_key_path);

    TEST_VIRTUAL_SPECIFIER void sendCthunError(
                    const std::string& request_id,
                    const std::string& description,
                    const std::vector<std::string>& endpoints);

    TEST_VIRTUAL_SPECIFIER void sendRPCError(
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
                    const ActionRequest& request,
                    const std::string& job_id,
                    const std::string& error);
};

}  // namespace CthunAgent

#endif  // SRC_AGENT_CTHUN_CONNECTOR_H_
