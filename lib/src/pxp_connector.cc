#include <pxp-agent/pxp_connector.hpp>
#include <pxp-agent/pxp_schemas.hpp>

#include <cpp-pcp-client/protocol/schemas.hpp>

#include <leatherman/util/strings.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.pxp_connector"
#include <leatherman/logging/logging.hpp>

namespace PXPAgent {

namespace lth_jc = leatherman::json_container;
namespace lth_util = leatherman::util;

static const int DEFAULT_MSG_TIMEOUT_SEC { 2 };

std::vector<lth_jc::JsonContainer> wrapDebug(
        const PCPClient::ParsedChunks& parsed_chunks) {
    auto request_id = parsed_chunks.envelope.get<std::string>("id");
    if (parsed_chunks.num_invalid_debug) {
        LOG_WARNING("Message %1% contained %2% bad debug chunk%3%",
                    request_id, parsed_chunks.num_invalid_debug,
                    lth_util::plural(parsed_chunks.num_invalid_debug));
    }
    std::vector<lth_jc::JsonContainer> debug {};
    for (auto& debug_entry : parsed_chunks.debug) {
        debug.push_back(debug_entry);
    }
    return debug;
}

PXPConnector::PXPConnector(const Configuration::Agent& agent_configuration)
        : PCPClient::Connector { agent_configuration.broker_ws_uri,
                                 agent_configuration.client_type,
                                 agent_configuration.ca,
                                 agent_configuration.crt,
                                 agent_configuration.key,
                                 agent_configuration.connection_timeout} {
}

void PXPConnector::sendPCPError(const std::string& request_id,
                                const std::string& description,
                                const std::vector<std::string>& endpoints) {
    lth_jc::JsonContainer pcp_error_data {};
    pcp_error_data.set<std::string>("id", request_id);
    pcp_error_data.set<std::string>("description", description);

    try {
        send(endpoints,
             PCPClient::Protocol::ERROR_MSG_TYPE,
             DEFAULT_MSG_TIMEOUT_SEC,
             pcp_error_data);
        LOG_INFO("Replied to request %1% with a PCP error message",
                 request_id);
    } catch (PCPClient::connection_error& e) {
        LOG_ERROR("Failed to send PCP error message for request %1%: %3%",
                  request_id, e.what());
    }
}

void PXPConnector::sendPXPError(const ActionRequest& request,
                                const std::string& description) {
    lth_jc::JsonContainer pxp_error_data {};
    pxp_error_data.set<std::string>("transaction_id", request.transactionId());
    pxp_error_data.set<std::string>("id", request.id());
    pxp_error_data.set<std::string>("description", description);

    try {
        send(std::vector<std::string> { request.sender() },
             PXPSchemas::PXP_ERROR_MSG_TYPE,
             DEFAULT_MSG_TIMEOUT_SEC,
             pxp_error_data);
        LOG_INFO("Replied to %1% request %2% by %3%, transaction %4%, with "
                 "an PXP error message", requestTypeNames[request.type()],
                 request.id(), request.sender(), request.transactionId());
    } catch (PCPClient::connection_error& e) {
        LOG_ERROR("Failed to send PXP error message for %1% request %2% by "
                  "%3%, transaction %4% (no further sending attempts): %5%",
                  requestTypeNames[request.type()], request.id(),
                  request.sender(), request.transactionId(), description);
    }
}

void PXPConnector::sendBlockingResponse(const ActionRequest& request,
                                        const lth_jc::JsonContainer& results) {
    auto debug = wrapDebug(request.parsedChunks());
    lth_jc::JsonContainer response_data {};
    response_data.set<std::string>("transaction_id", request.transactionId());
    response_data.set<lth_jc::JsonContainer>("results", results);

    try {
        send(std::vector<std::string> { request.sender() },
             PXPSchemas::BLOCKING_RESPONSE_TYPE,
             DEFAULT_MSG_TIMEOUT_SEC,
             response_data,
             debug);
        LOG_INFO("Sent response for blocking request %1% by %2%, "
                 "transaction %3%", request.id(), request.sender(),
                 request.transactionId());
    } catch (PCPClient::connection_error& e) {
        LOG_ERROR("Failed to reply to blocking request %1% from %2%, "
                  "transaction %3%: %4%", request.id(), request.sender(),
                  request.transactionId(), e.what());
    }
}

void PXPConnector::sendNonBlockingResponse(const ActionRequest& request,
                                           const lth_jc::JsonContainer& results,
                                           const std::string& job_id) {
    lth_jc::JsonContainer response_data {};
    response_data.set<std::string>("transaction_id", request.transactionId());
    response_data.set<std::string>("job_id", job_id);
    response_data.set<lth_jc::JsonContainer>("results", results);

    try {
        // NOTE(ale): assuming debug was sent in provisional response
        send(std::vector<std::string> { request.sender() },
             PXPSchemas::NON_BLOCKING_RESPONSE_TYPE,
             DEFAULT_MSG_TIMEOUT_SEC,
             response_data);
        LOG_INFO("Sent response for non-blocking request %1% by %2%, "
                 "transaction %3%", request.id(), request.sender(),
                 request.transactionId());
    } catch (PCPClient::connection_error& e) {
        LOG_ERROR("Failed to reply to non-blocking request %1% by %2%, "
                  "transaction %3% (no further attempts): %4%",
                  request.id(), request.sender(), request.transactionId(),
                  e.what());
    }
}

void PXPConnector::sendProvisionalResponse(const ActionRequest& request) {
    auto debug = wrapDebug(request.parsedChunks());
    lth_jc::JsonContainer provisional_data {};
    provisional_data.set<std::string>("transaction_id", request.transactionId());

    try {
        send(std::vector<std::string> { request.sender() },
             PXPSchemas::PROVISIONAL_RESPONSE_TYPE,
             DEFAULT_MSG_TIMEOUT_SEC,
             provisional_data,
             debug);
        LOG_INFO("Sent provisional response for request %1% by %2%, "
                 "transaction %3%", request.id(), request.sender(),
                 request.transactionId());
    } catch (PCPClient::connection_error& e) {
        LOG_ERROR("Failed to send provisional response for request %1% by "
                  "%2%, transaction %3% (no further attempts): %4%",
                  request.id(), request.sender(), request.transactionId(), e.what());
    }
}

}  // namesapce PXPAgent
