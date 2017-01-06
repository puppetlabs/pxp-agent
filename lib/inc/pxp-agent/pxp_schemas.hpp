#ifndef SRC_AGENT_PXP_SCHEMAS_HPP_
#define SRC_AGENT_PXP_SCHEMAS_HPP_

#include <leatherman/json_container/schema.hpp>

namespace PXPAgent {
namespace PXPSchemas {

// PXP blocking transaction
static const std::string BLOCKING_REQUEST_TYPE  {
    "http://puppetlabs.com/rpc_blocking_request" };
static const std::string BLOCKING_RESPONSE_TYPE {
    "http://puppetlabs.com/rpc_blocking_response" };
leatherman::json_container::Schema BlockingRequestSchema();
leatherman::json_container::Schema BlockingResponseSchema();

// PXP non blocking transaction
static const std::string NON_BLOCKING_REQUEST_TYPE  {
    "http://puppetlabs.com/rpc_non_blocking_request" };
static const std::string NON_BLOCKING_RESPONSE_TYPE {
    "http://puppetlabs.com/rpc_non_blocking_response" };
static const std::string PROVISIONAL_RESPONSE_TYPE  {
    "http://puppetlabs.com/rpc_provisional_response" };
leatherman::json_container::Schema NonBlockingRequestSchema();
leatherman::json_container::Schema NonBlockingResponseSchema();
leatherman::json_container::Schema ProvisionalResponseSchema();

// PXP error
static const std::string PXP_ERROR_MSG_TYPE {
    "http://puppetlabs.com/rpc_error_message" };
leatherman::json_container::Schema PXPErrorSchema();

}  // namespace PXPSchemas
}  // namespace PXPAgent

#endif  // SRC_AGENT_PXP_SCHEMAS_HPP_
