#include <pxp-agent/pxp_connector.hpp>
#include <pxp-agent/pxp_schemas.hpp>

#include <cpp-pcp-client/protocol/schemas.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.pxp_connector"
#include <leatherman/logging/logging.hpp>

#include <utility>

namespace PXPAgent {

namespace lth_jc = leatherman::json_container;

std::vector<lth_jc::JsonContainer>
wrapDebug(const PCPClient::ParsedChunks& parsed_chunks)
{
    auto request_id = parsed_chunks.envelope.get<std::string>("id");
    if (parsed_chunks.num_invalid_debug) {
        // TODO(ale): deal with locale & plural (PCP-257)
        if (parsed_chunks.num_invalid_debug == 1) {
            LOG_WARNING("Message {1} contained {2} bad debug chunk",
                        request_id, parsed_chunks.num_invalid_debug);
        } else {
            LOG_WARNING("Message {1} contained {2} bad debug chunks",
                        request_id, parsed_chunks.num_invalid_debug);
        }
    }
    std::vector<lth_jc::JsonContainer> debug {};
    for (auto& debug_entry : parsed_chunks.debug) {
        debug.push_back(debug_entry);
    }
    return debug;
}

static std::vector<std::string> join_broker_ws_uris(std::string const& a, std::string const& b)
{
    if (b.empty())
        return std::vector<std::string>({a});
    else
        return std::vector<std::string>({a, b});
}

PXPConnector::PXPConnector(const Configuration::Agent& agent_configuration)
        : PCPClient::Connector(join_broker_ws_uris(agent_configuration.broker_ws_uri,
                                                   agent_configuration.failover_ws_uri),
                               agent_configuration.client_type,
                               agent_configuration.ca,
                               agent_configuration.crt,
                               agent_configuration.key,
                               agent_configuration.ws_connection_timeout_ms,
                               agent_configuration.association_timeout_s,
                               agent_configuration.association_request_ttl_s,
                               agent_configuration.allowed_keepalive_timeouts+1),
          pcp_message_ttl_s { agent_configuration.pcp_message_ttl_s }
{
}

void PXPConnector::sendPCPError(const std::string& request_id,
                                const std::string& description,
                                const std::vector<std::string>& endpoints)
{
    lth_jc::JsonContainer pcp_error_data {};
    pcp_error_data.set<std::string>("id", request_id);
    pcp_error_data.set<std::string>("description", description);

    try {
        send(endpoints,
             PCPClient::Protocol::ERROR_MSG_TYPE,
             pcp_message_ttl_s,
             pcp_error_data);
        LOG_INFO("Replied to request {1} with a PCP error message",
                 request_id);
    } catch (PCPClient::connection_error& e) {
        LOG_ERROR("Failed to send PCP error message for request {1}: {2}",
                  request_id, e.what());
    }
}

void PXPConnector::sendProvisionalResponse(const ActionRequest& request)
{
    auto debug = wrapDebug(request.parsedChunks());
    lth_jc::JsonContainer provisional_data {};
    provisional_data.set<std::string>("transaction_id", request.transactionId());

    try {
        send(std::vector<std::string> { request.sender() },
             PXPSchemas::PROVISIONAL_RESPONSE_TYPE,
             pcp_message_ttl_s,
             provisional_data,
             debug);
        LOG_INFO("Sent provisional response for the {1} by {2}",
                 request.prettyLabel(), request.sender());
    } catch (PCPClient::connection_error& e) {
        LOG_ERROR("Failed to send provisional response for the {1} by {2} "
                  "(no further attempts will be made): {3}",
                  request.prettyLabel(), request.sender(), e.what());
    }
}

void PXPConnector::sendPXPError(const ActionRequest& request,
                                const std::string& description)
{
    lth_jc::JsonContainer pxp_error_data {};
    pxp_error_data.set<std::string>("transaction_id", request.transactionId());
    pxp_error_data.set<std::string>("id", request.id());
    pxp_error_data.set<std::string>("description", description);

    try {
        send(std::vector<std::string> { request.sender() },
             PXPSchemas::PXP_ERROR_MSG_TYPE,
             pcp_message_ttl_s,
             pxp_error_data);
        LOG_INFO("Replied to {1} by {2}, request ID {3}, with a PXP error message",
                 request.prettyLabel(), request.sender(), request.id());
    } catch (PCPClient::connection_error& e) {
        LOG_ERROR("Failed to send a PXP error message for the {1} by {2} "
                  "(no further sending attempts will be made): {3}",
                  request.prettyLabel(), request.sender(), description);
    }
}

void PXPConnector::sendPXPError(const ActionResponse& response)
{
    assert(response.valid(ActionResponse::ResponseType::RPCError));

    try {
        send(std::vector<std::string> {
                response.action_metadata.get<std::string>("requester") },
             PXPSchemas::PXP_ERROR_MSG_TYPE,
             pcp_message_ttl_s,
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

void PXPConnector::sendBlockingResponse(const ActionResponse& response,
                                        const ActionRequest& request)
{
    assert(response.valid(ActionResponse::ResponseType::Blocking));
    sendBlockingResponse_(ActionResponse::ResponseType::Blocking,
                          response,
                          request);
}

void PXPConnector::sendStatusResponse(const ActionResponse& response,
                                      const ActionRequest& request)
{
    assert(response.valid(ActionResponse::ResponseType::StatusOutput));
    sendBlockingResponse_(ActionResponse::ResponseType::StatusOutput,
                          response,
                          request);
}

void PXPConnector::sendNonBlockingResponse(const ActionResponse& response)
{
    assert(response.valid(ActionResponse::ResponseType::NonBlocking));
    assert(response.action_metadata.get<std::string>("status") != "undetermined");

    try {
        // NOTE(ale): assuming debug was sent in provisional response
        send(std::vector<std::string> {
                response.action_metadata.get<std::string>("requester") },
             PXPSchemas::NON_BLOCKING_RESPONSE_TYPE,
             pcp_message_ttl_s,
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

//
// Private interface
//

void PXPConnector::sendBlockingResponse_(
        const ActionResponse::ResponseType& response_type,
        const ActionResponse& response,
        const ActionRequest& request)
{
    auto debug = wrapDebug(request.parsedChunks());

    try {
        send(std::vector<std::string> { request.sender() },
             PXPSchemas::BLOCKING_RESPONSE_TYPE,
             pcp_message_ttl_s,
             response.toJSON(response_type),
             debug);
        LOG_INFO("Sent response for the {1} by {2}",
                 request.prettyLabel(), request.sender());
    } catch (PCPClient::connection_error& e) {
        LOG_ERROR("Failed to reply to the {1} by {2}: {3}",
                  request.prettyLabel(), request.sender(), e.what());
    }
}

}  // namesapce PXPAgent
