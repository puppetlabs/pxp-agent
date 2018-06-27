#include <pxp-agent/pxp_connector_v2.hpp>
#include <pxp-agent/pxp_schemas.hpp>

#include <leatherman/json_container/json_container.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.pxp_connector"
#include <leatherman/logging/logging.hpp>

#include <utility>

namespace PXPAgent {

namespace lth_jc = leatherman::json_container;

//
// PXPConnector V2
//

PXPConnectorV2::PXPConnectorV2(const Configuration::Agent& agent_configuration)
        : PCPClient::v2::Connector(agent_configuration.broker_ws_uris,
                                   agent_configuration.client_type,
                                   agent_configuration.ca,
                                   agent_configuration.crt,
                                   agent_configuration.key,
                                   agent_configuration.broker_ws_proxy,
                                   agent_configuration.ws_connection_timeout_ms,
                                   agent_configuration.allowed_keepalive_timeouts+1)
{
}

void PXPConnectorV2::sendPCPError(const std::string& request_id,
                                  const std::string& description,
                                  const std::vector<std::string>& endpoints)
{
    try {
        sendError(endpoints.front(),
                  request_id,
                  description);
        LOG_INFO("Replied to request {1} with a PCP error message",
                 request_id);
    } catch (PCPClient::connection_error& e) {
        LOG_ERROR("Failed to send PCP error message for request {1}: {2}",
                  request_id, e.what());
    }
}

void PXPConnectorV2::sendProvisionalResponse(const ActionRequest& request)
{
    lth_jc::JsonContainer provisional_data {};
    provisional_data.set<std::string>("transaction_id", request.transactionId());

    try {
        send(request.sender(),
             PXPSchemas::PROVISIONAL_RESPONSE_TYPE,
             provisional_data);
        LOG_INFO("Sent provisional response for the {1} by {2}",
                 request.prettyLabel(), request.sender());
    } catch (PCPClient::connection_error& e) {
        LOG_ERROR("Failed to send provisional response for the {1} by {2} "
                  "(no further attempts will be made): {3}",
                  request.prettyLabel(), request.sender(), e.what());
    }
}

void PXPConnectorV2::sendPXPError(const ActionRequest& request,
                                  const std::string& description)
{
    lth_jc::JsonContainer pxp_error_data {};
    pxp_error_data.set<std::string>("transaction_id", request.transactionId());
    pxp_error_data.set<std::string>("id", request.id());
    pxp_error_data.set<std::string>("description", description);

    try {
        send(request.sender(),
             PXPSchemas::PXP_ERROR_MSG_TYPE,
             pxp_error_data);
        LOG_INFO("Replied to {1} by {2}, request ID {3}, with a PXP error message",
                 request.prettyLabel(), request.sender(), request.id());
    } catch (PCPClient::connection_error& e) {
        LOG_ERROR("Failed to send a PXP error message for the {1} by {2} "
                  "(no further sending attempts will be made): {3}",
                  request.prettyLabel(), request.sender(), description);
    }
}

void PXPConnectorV2::sendPXPError(const ActionResponse& response)
{
    assert(response.valid(ActionResponse::ResponseType::RPCError));

    try {
        send(response.action_metadata.get<std::string>("requester"),
             PXPSchemas::PXP_ERROR_MSG_TYPE,
             response.toJSON(ActionResponse::ResponseType::RPCError));
        LOG_INFO("Replied to {1} by {2}, request ID {3}, with a PXP error message",
                 response.prettyRequestLabel(),
                 response.action_metadata.get<std::string>("requester"),
                 response.action_metadata.get<std::string>("request_id"));
    } catch (PCPClient::connection_error& e) {
        LOG_ERROR("Failed to send a PXP error message for the {1} by {2} "
                  "(no further sending attempts will be made): {3}",
                  response.prettyRequestLabel(),
                  response.action_metadata.get<std::string>("requester"),
                  response.action_metadata.get<std::string>("execution_error"));
    }
}

void PXPConnectorV2::sendBlockingResponse(const ActionResponse& response,
                                          const ActionRequest& request)
{
    assert(response.valid(ActionResponse::ResponseType::Blocking));
    sendBlockingResponse_(ActionResponse::ResponseType::Blocking,
                          response,
                          request);
}

void PXPConnectorV2::sendStatusResponse(const ActionResponse& response,
                                        const ActionRequest& request)
{
    assert(response.valid(ActionResponse::ResponseType::StatusOutput));
    sendBlockingResponse_(ActionResponse::ResponseType::StatusOutput,
                          response,
                          request);
}

void PXPConnectorV2::sendNonBlockingResponse(const ActionResponse& response)
{
    assert(response.valid(ActionResponse::ResponseType::NonBlocking));
    assert(response.action_metadata.get<std::string>("status") != "undetermined");

    try {
        // NOTE(ale): assuming debug was sent in provisional response
        send(response.action_metadata.get<std::string>("requester"),
             PXPSchemas::NON_BLOCKING_RESPONSE_TYPE,
             response.toJSON(ActionResponse::ResponseType::NonBlocking));
        LOG_INFO("Sent response for the {1} by {2}",
                 response.prettyRequestLabel(),
                 response.action_metadata.get<std::string>("requester"));
    } catch (PCPClient::connection_error& e) {
        LOG_ERROR("Failed to reply to {1} by {2}, (no further attempts will "
                  "be made): {3}",
                  response.prettyRequestLabel(),
                  response.action_metadata.get<std::string>("requester"),
                  e.what());
    }
}

void PXPConnectorV2::connect(int max_connect_attempts)
{
    PCPClient::v2::Connector::connect(max_connect_attempts);
}

void PXPConnectorV2::monitorConnection(uint32_t max_connect_attempts,
                                       uint32_t connection_check_interval_s)
{
    PCPClient::v2::Connector::monitorConnection(max_connect_attempts, connection_check_interval_s);
}

void PXPConnectorV2::registerMessageCallback(const PCPClient::Schema& schema,
                                             MessageCallback callback)
{
    PCPClient::v2::Connector::registerMessageCallback(schema, callback);
}

//
// Private interface
//

void PXPConnectorV2::sendBlockingResponse_(
        const ActionResponse::ResponseType& response_type,
        const ActionResponse& response,
        const ActionRequest& request)
{
    try {
        send(request.sender(),
             PXPSchemas::BLOCKING_RESPONSE_TYPE,
             response.toJSON(response_type));
        LOG_INFO("Sent response for the {1} by {2}",
                 request.prettyLabel(), request.sender());
    } catch (PCPClient::connection_error& e) {
        LOG_ERROR("Failed to reply to the {1} by {2}: {3}",
                  request.prettyLabel(), request.sender(), e.what());
    }
}

}  // namesapce PXPAgent
