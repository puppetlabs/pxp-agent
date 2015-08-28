#ifndef SRC_AGENT_RPC_SCHEMAS_H_
#define SRC_AGENT_RPC_SCHEMAS_H_

#include <cthun-client/validator/schema.hpp>

// TODO(ale): move this to separate cthun-rpc library

namespace CthunAgent {
namespace RPCSchemas {

// RPC blocking transaction
static const std::string BLOCKING_REQUEST_TYPE  {
    "http://puppetlabs.com/rpc_blocking_request" };
static const std::string BLOCKING_RESPONSE_TYPE {
    "http://puppetlabs.com/rpc_blocking_response" };
CthunClient::Schema BlockingRequestSchema();
CthunClient::Schema BlockingResponseSchema();

// RPC non blocking transaction
static const std::string NON_BLOCKING_REQUEST_TYPE  {
    "http://puppetlabs.com/rpc_non_blocking_request" };
static const std::string NON_BLOCKING_RESPONSE_TYPE {
    "http://puppetlabs.com/rpc_non_blocking_response" };
static const std::string PROVISIONAL_RESPONSE_TYPE  {
    "http://puppetlabs.com/rpc_provisional_response" };
CthunClient::Schema NonBlockingRequestSchema();
CthunClient::Schema NonBlockingResponseSchema();
CthunClient::Schema ProvisionalResponseSchema();

// RPC error
static const std::string RPC_ERROR_MSG_TYPE {
    "http://puppetlabs.com/rpc_error_message" };
CthunClient::Schema RPCErrorSchema();

}  // namespace RPCSchemas
}  // namespace CthunAgent

#endif  // SRC_AGENT_RPC_SCHEMAS_H_
