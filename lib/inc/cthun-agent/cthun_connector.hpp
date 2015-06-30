#ifndef SRC_AGENT_CTHUN_CONNECTOR_H_
#define SRC_AGENT_CTHUN_CONNECTOR_H_

#include <cthun-agent/action_request.hpp>

#include <cthun-client/connector/connector.hpp>

#include <leatherman/json_container/json_container.hpp>

#include <cassert>
#include <memory>

namespace CthunAgent {

namespace LTH_JC = leatherman::json_container;

class CthunConnector : public CthunClient::Connector {
  public:
    CthunConnector(const std::string& server_url,
                   const std::string& client_type,
                   const std::string& ca_crt_path,
                   const std::string& client_crt_path,
                   const std::string& client_key_path);

    // The following methods are virtual for testing purpose

    virtual void sendCthunError(const std::string& request_id,
                                const std::string& description,
                                const std::vector<std::string>& endpoints);

    virtual void sendRPCError(const ActionRequest& request,
                              const std::string& description);

    virtual void sendBlockingResponse(const ActionRequest& request,
                                      const LTH_JC::JsonContainer& results);

    virtual void sendNonBlockingResponse(const ActionRequest& request,
                                         const LTH_JC::JsonContainer& results,
                                         const std::string& job_id);

    virtual void sendProvisionalResponse(const ActionRequest& request,
                                         const std::string& job_id,
                                         const std::string& error);
};

}  // namespace CthunAgent

#endif  // SRC_AGENT_CTHUN_CONNECTOR_H_
