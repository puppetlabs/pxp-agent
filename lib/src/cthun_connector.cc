#include <cthun-agent/cthun_connector.hpp>
#include <cthun-agent/rpc_schemas.hpp>

#include <cthun-client/protocol/schemas.hpp>

#include <leatherman/util/strings.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.cthun_agent.cthun_connector"
#include <leatherman/logging/logging.hpp>

namespace CthunAgent {

namespace lth_jc = leatherman::json_container;
namespace lth_util = leatherman::util;

static const int DEFAULT_MSG_TIMEOUT_SEC { 2 };

std::vector<lth_jc::JsonContainer> wrapDebug(
                        const CthunClient::ParsedChunks& parsed_chunks) {
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

CthunConnector::CthunConnector(const Configuration::Agent& agent_configuration)
        : CthunClient::Connector { agent_configuration.server_url,
                                   agent_configuration.client_type,
                                   agent_configuration.ca,
                                   agent_configuration.crt,
                                   agent_configuration.key } {
}

void CthunConnector::sendCthunError(const std::string& request_id,
                                    const std::string& description,
                                    const std::vector<std::string>& endpoints) {
    lth_jc::JsonContainer cthun_error_data {};
    cthun_error_data.set<std::string>("id", request_id);
    cthun_error_data.set<std::string>("description", description);

    try {
        send(endpoints,
             CthunClient::Protocol::ERROR_MSG_TYPE,
             DEFAULT_MSG_TIMEOUT_SEC,
             cthun_error_data);
        LOG_INFO("Replied to request %1% with a Cthun core error message",
                 request_id);
    } catch (CthunClient::connection_error& e) {
        LOG_ERROR("Failed to send Cthun core error message for request %1%: %3%",
                  request_id, e.what());
    }
}

void CthunConnector::sendRPCError(const ActionRequest& request,
                                  const std::string& description) {
    lth_jc::JsonContainer rpc_error_data {};
    rpc_error_data.set<std::string>("transaction_id", request.transactionId());
    rpc_error_data.set<std::string>("id", request.id());
    rpc_error_data.set<std::string>("description", description);

    try {
        send(std::vector<std::string> { request.sender() },
             RPCSchemas::RPC_ERROR_MSG_TYPE,
             DEFAULT_MSG_TIMEOUT_SEC,
             rpc_error_data);
        LOG_INFO("Replied to %1% request %2% by %3%, transaction %4%, with "
                 "an RPC error message", requestTypeNames[request.type()],
                 request.id(), request.sender(), request.transactionId());
    } catch (CthunClient::connection_error& e) {
        LOG_ERROR("Failed to send RPC error message for %1% request %2% by "
                  "%3%, transaction %4% (no further sending attempts): %5%",
                  requestTypeNames[request.type()], request.id(),
                  request.sender(), request.transactionId(), description);
    }
}

void CthunConnector::sendBlockingResponse(
                                const ActionRequest& request,
                                const lth_jc::JsonContainer& results) {
    auto debug = wrapDebug(request.parsedChunks());
    lth_jc::JsonContainer response_data {};
    response_data.set<std::string>("transaction_id", request.transactionId());
    response_data.set<lth_jc::JsonContainer>("results", results);

    try {
        send(std::vector<std::string> { request.sender() },
             RPCSchemas::BLOCKING_RESPONSE_TYPE,
             DEFAULT_MSG_TIMEOUT_SEC,
             response_data,
             debug);
    } catch (CthunClient::connection_error& e) {
        LOG_ERROR("Failed to reply to blocking request %1% from %2%, "
                  "transaction %3%: %4%", request.id(), request.sender(),
                  request.transactionId(), e.what());
    }
}

void CthunConnector::sendNonBlockingResponse(
                                const ActionRequest& request,
                                const lth_jc::JsonContainer& results,
                                const std::string& job_id) {
    lth_jc::JsonContainer response_data {};
    response_data.set<std::string>("transaction_id", request.transactionId());
    response_data.set<std::string>("job_id", job_id);
    response_data.set<lth_jc::JsonContainer>("results", results);

    try {
        // NOTE(ale): assuming debug was sent in provisional response
        send(std::vector<std::string> { request.sender() },
             RPCSchemas::NON_BLOCKING_RESPONSE_TYPE,
             DEFAULT_MSG_TIMEOUT_SEC,
             response_data);
        LOG_INFO("Sent response for non-blocking request %1% by %2%, "
                 "transaction %3%", request.id(), request.sender(),
                 request.transactionId());
    } catch (CthunClient::connection_error& e) {
        LOG_ERROR("Failed to reply to non-blocking request %1% by %2%, "
                  "transaction %3% (no further attempts): %4%",
                  request.id(), request.sender(), request.transactionId(),
                  e.what());
    }
}

void CthunConnector::sendProvisionalResponse(const ActionRequest& request) {
    auto debug = wrapDebug(request.parsedChunks());
    lth_jc::JsonContainer provisional_data {};
    provisional_data.set<std::string>("transaction_id", request.transactionId());

    try {
        send(std::vector<std::string> { request.sender() },
             RPCSchemas::PROVISIONAL_RESPONSE_TYPE,
             DEFAULT_MSG_TIMEOUT_SEC,
             provisional_data,
             debug);
        LOG_INFO("Sent provisional response for request %1% by %2%, "
                 "transaction %3%", request.id(), request.sender(),
                 request.transactionId());
    } catch (CthunClient::connection_error& e) {
        LOG_ERROR("Failed to send provisional response for request %1% by "
                  "%2%, transaction %3% (no further attempts): %4%",
                  request.id(), request.sender(), request.transactionId(), e.what());
    }
}

}  // namesapce CthunAgent
