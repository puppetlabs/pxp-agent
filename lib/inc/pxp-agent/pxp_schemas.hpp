#ifndef SRC_AGENT_PXP_SCHEMAS_HPP_
#define SRC_AGENT_PXP_SCHEMAS_HPP_

#include <cpp-pcp-client/validator/schema.hpp>

// TODO(ale): move this to separate cpp-pxp-client library

namespace PXPAgent {
namespace PXPSchemas {

// PXP blocking transaction
static const std::string BLOCKING_REQUEST_TYPE  {
    "http://puppetlabs.com/rpc_blocking_request" };
static const std::string BLOCKING_RESPONSE_TYPE {
    "http://puppetlabs.com/rpc_blocking_response" };
PCPClient::Schema BlockingRequestSchema();
PCPClient::Schema BlockingResponseSchema();

// PXP non blocking transaction
static const std::string NON_BLOCKING_REQUEST_TYPE  {
    "http://puppetlabs.com/rpc_non_blocking_request" };
static const std::string NON_BLOCKING_RESPONSE_TYPE {
    "http://puppetlabs.com/rpc_non_blocking_response" };
static const std::string PROVISIONAL_RESPONSE_TYPE  {
    "http://puppetlabs.com/rpc_provisional_response" };
PCPClient::Schema NonBlockingRequestSchema();
PCPClient::Schema NonBlockingResponseSchema();
PCPClient::Schema ProvisionalResponseSchema();

// PXP error
static const std::string PXP_ERROR_MSG_TYPE {
    "http://puppetlabs.com/rpc_error_message" };
PCPClient::Schema PXPErrorSchema();

}  // namespace PXPSchemas
}  // namespace PXPAgent

#endif  // SRC_AGENT_PXP_SCHEMAS_HPP_
