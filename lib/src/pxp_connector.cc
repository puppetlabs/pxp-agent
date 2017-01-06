#include <pxp-agent/pxp_connector.hpp>
#include <pxp-agent/pxp_schemas.hpp>
#include <pxp-agent/agent.hpp>

#include <cpp-pcp-client/connector/connector.hpp>
#include <cpp-pcp-client/protocol/schemas.hpp>

#include <leatherman/util/thread.hpp>   // this_thread::sleep_for
#include <leatherman/util/chrono.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.pxp_connector"
#include <leatherman/logging/logging.hpp>

#include <utility>
#include <vector>
#include <cstdint>
#include <random>

namespace PXPAgent {

namespace lth_jc = leatherman::json_container;
namespace lth_util = leatherman::util;
namespace lth_loc = leatherman::locale;

// Pause between connection attempts after Association errors
static const uint32_t ASSOCIATE_SESSION_TIMEOUT_PAUSE_S { 5 };

PXPConnector::PXPConnector(const Configuration::Agent& agent_configuration)
    try
        : pcp_message_ttl_s { agent_configuration.pcp_message_ttl_s },
          pcp_connector_ { new PCPClient::Connector(agent_configuration.broker_ws_uris,
                                                    agent_configuration.client_type,
                                                    agent_configuration.ca,
                                                    agent_configuration.crt,
                                                    agent_configuration.key,
                                                    agent_configuration.ws_connection_timeout_ms,
                                                    agent_configuration.association_timeout_s,
                                                    agent_configuration.association_request_ttl_s,
                                                    agent_configuration.allowed_keepalive_timeouts+1) }
{
} catch (const PCPClient::connection_config_error& e) {
    throw Agent::WebSocketConfigurationError { e.what() };
}

void PXPConnector::sendProvisionalResponse(const ActionRequest& request)
{
    assert(pcp_connector_);

    lth_jc::JsonContainer provisional_data {};
    provisional_data.set<std::string>("transaction_id", request.transactionId());

    try {
        pcp_connector_->send(std::vector<std::string> { request.sender() },
                             PXPSchemas::PROVISIONAL_RESPONSE_TYPE,
                             pcp_message_ttl_s,
                             provisional_data,
                             request.debug());
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
    assert(pcp_connector_);

    lth_jc::JsonContainer pxp_error_data {};
    pxp_error_data.set<std::string>("transaction_id", request.transactionId());
    pxp_error_data.set<std::string>("id", request.id());
    pxp_error_data.set<std::string>("description", description);

    try {
        pcp_connector_->send(std::vector<std::string> { request.sender() },
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
    assert(pcp_connector_);

    try {
        pcp_connector_->send(std::vector<std::string> {
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
    assert(pcp_connector_);

    sendBlockingResponse_(ActionResponse::ResponseType::Blocking,
                          response,
                          request);
}

void PXPConnector::sendStatusResponse(const ActionResponse& response,
                                      const ActionRequest& request)
{
    assert(response.valid(ActionResponse::ResponseType::StatusOutput));
    assert(pcp_connector_);

    sendBlockingResponse_(ActionResponse::ResponseType::StatusOutput,
                          response,
                          request);
}

void PXPConnector::sendNonBlockingResponse(const ActionResponse& response)
{
    assert(response.valid(ActionResponse::ResponseType::NonBlocking));
    assert(response.action_metadata.get<std::string>("status") != "undetermined");
    assert(pcp_connector_);

    try {
        // NOTE(ale): assuming debug was sent in provisional response
        pcp_connector_->send(std::vector<std::string> {
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

void PXPConnector::connect()
{
    assert(pcp_connector_);

    int num_seconds {};
    std::random_device rd {};
    std::default_random_engine engine { rd() };
    std::uniform_int_distribution<int> dist { ASSOCIATE_SESSION_TIMEOUT_PAUSE_S,
                                              2 * ASSOCIATE_SESSION_TIMEOUT_PAUSE_S };

    try {
        do {
            try {
                pcp_connector_->connect();
                break;
            } catch (const PCPClient::connection_association_error& e) {
                num_seconds = dist(engine);
                LOG_WARNING("Error during the PCP Session Association ({1}); "
                            "will retry to connect in {2} s",
                            e.what(), num_seconds);
                lth_util::this_thread::sleep_for(
                    lth_util::chrono::seconds(num_seconds));
            }
        } while (true);

        // The agent is now connected and the request handlers are
        // set; we can now call the monitoring method that will block
        // this thread of execution.
        // Note that, in case the underlying connection drops, the
        // connector will keep trying to re-establish it indefinitely
        // (the max_connect_attempts is 0 by default).
        LOG_DEBUG("PCP connection established; about to start monitoring it");
        pcp_connector_->monitorConnection();
    } catch (const PCPClient::connection_config_error& e) {
        // WebSocket configuration failure
        throw Agent::WebSocketConfigurationError { e.what() };
    } catch (const PCPClient::connection_association_response_failure& e) {
        // Associate Session failure; this should be due to a
        // configuration mismatch with the broker (incompatible
        // PCP versions?)
        throw Agent::ConfigurationError { e.what() };
    } catch (const PCPClient::connection_fatal_error& e) {
        // Unexpected, as we're not limiting the num retries, for
        // both connect() and monitorConnection() calls
        throw Agent::FatalError { e.what() };
    }
}

void PXPConnector::registerMessageCallback(const leatherman::json_container::Schema& schema,
                                           MessageCallback callback)
{
    assert(pcp_connector_);

    pcp_connector_->registerMessageCallback(
        schema,
        [this, callback](const PCPClient::ParsedChunks& parsed_chunks) {
            auto id = parsed_chunks.envelope.get<std::string>("id");
            auto sender = parsed_chunks.envelope.get<std::string>("sender");
            if (validateFormat_(id, sender, parsed_chunks)) {
                if (parsed_chunks.num_invalid_debug) {
                    LOG_WARNING(lth_loc::format_n(
                            "Message {1} contained {2} bad debug chunk",
                            "Message {1} contained {2} bad debug chunks",
                            parsed_chunks.num_invalid_debug, parsed_chunks.num_invalid_debug, id));
                }

                callback(id, sender, parsed_chunks.data, parsed_chunks.debug);
            }
        });
}


//
// Private interface
//

void PXPConnector::sendBlockingResponse_(
        const ActionResponse::ResponseType& response_type,
        const ActionResponse& response,
        const ActionRequest& request)
{
    try {
        pcp_connector_->send(std::vector<std::string> { request.sender() },
                             PXPSchemas::BLOCKING_RESPONSE_TYPE,
                             pcp_message_ttl_s,
                             response.toJSON(response_type),
                             request.debug());
        LOG_INFO("Sent response for the {1} by {2}",
                 request.prettyLabel(), request.sender());
    } catch (PCPClient::connection_error& e) {
        LOG_ERROR("Failed to reply to the {1} by {2}: {3}",
                  request.prettyLabel(), request.sender(), e.what());
    }
}

void PXPConnector::sendPCPError_(const std::string& request_id,
                                 const std::string& description,
                                 const std::vector<std::string>& endpoints)
{
    lth_jc::JsonContainer pcp_error_data {};
    pcp_error_data.set<std::string>("id", request_id);
    pcp_error_data.set<std::string>("description", description);

    try {
        pcp_connector_->send(endpoints,
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

bool PXPConnector::validateFormat_(const std::string& id,
                                   const std::string& sender,
                                   const PCPClient::ParsedChunks& parsed_chunks) {
    std::string errmsg;
    if (!parsed_chunks.has_data)
        errmsg = lth_loc::translate("no data");
    if (parsed_chunks.invalid_data)
        errmsg = lth_loc::translate("invalid data");
    // NOTE(ale): currently, we don't support ContentType::Binary
    if (parsed_chunks.data_type != lth_jc::ContentType::Json)
        errmsg = lth_loc::translate("data is not in JSON format");

    if (errmsg.empty())
        return true;

    // Message was not a valid action, send a *PCP error*
    std::vector<std::string> endpoints { sender };
    LOG_ERROR("Invalid request with ID {1} by {2}. Will reply with a PCP "
              "error. Error: {3}", id, sender, errmsg);
    sendPCPError_(id, errmsg, endpoints);
    return false;
}

}  // namesapce PXPAgent
