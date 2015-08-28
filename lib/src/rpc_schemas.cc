#include <pxp-agent/rpc_schemas.hpp>

#include <cpp-pcp-client/protocol/schemas.hpp>

namespace PXPAgent {
namespace RPCSchemas {

// HERE(ale): this must be kept up to date with
// https://github.com/puppetlabs/cthun-specifications

using C_Type = PCPClient::ContentType;
using T_Constraint = PCPClient::TypeConstraint;

PCPClient::Schema BlockingRequestSchema() {
    PCPClient::Schema schema { BLOCKING_REQUEST_TYPE, C_Type::Json };
    // NB: additionalProperties = false
    schema.addConstraint("transaction_id", T_Constraint::String, true);
    schema.addConstraint("module", T_Constraint::String, true);
    schema.addConstraint("action", T_Constraint::String, true);
    schema.addConstraint("params", T_Constraint::Object, false);
    return schema;
}

PCPClient::Schema BlockingResponseSchema() {
    PCPClient::Schema schema { BLOCKING_RESPONSE_TYPE, C_Type::Json };
    // NB: additionalProperties = false
    schema.addConstraint("transaction_id", T_Constraint::String, true);
    schema.addConstraint("results", T_Constraint::Object, true);
    return schema;
}

PCPClient::Schema NonBlockingRequestSchema() {
    PCPClient::Schema schema { NON_BLOCKING_REQUEST_TYPE, C_Type::Json };
    // NB: additionalProperties = false
    schema.addConstraint("transaction_id", T_Constraint::String, true);
    schema.addConstraint("notify_outcome", T_Constraint::Bool, true);
    schema.addConstraint("module", T_Constraint::String, true);
    schema.addConstraint("action", T_Constraint::String, true);
    schema.addConstraint("params", T_Constraint::Object, false);
    return schema;
}

PCPClient::Schema NonBlockingResponseSchema() {
    PCPClient::Schema schema { NON_BLOCKING_RESPONSE_TYPE, C_Type::Json };
    // NB: additionalProperties = false
    schema.addConstraint("transaction_id", T_Constraint::String, true);
    schema.addConstraint("job_id", T_Constraint::String, true);
    schema.addConstraint("results", T_Constraint::Object, true);
    return schema;
}

PCPClient::Schema ProvisionalResponseSchema() {
    PCPClient::Schema schema { PROVISIONAL_RESPONSE_TYPE, C_Type::Json };
    // NB: additionalProperties = false
    schema.addConstraint("transaction_id", T_Constraint::String, true);
    return schema;
}

PCPClient::Schema RPCErrorSchema() {
    PCPClient::Schema schema { RPC_ERROR_MSG_TYPE, C_Type::Json };
    // NB: additionalProperties = false
    schema.addConstraint("transaction_id", T_Constraint::String, true);
    schema.addConstraint("id", T_Constraint::String, true);
    schema.addConstraint("description", T_Constraint::String, true);
    return schema;
}

}  // namespace PXPAgent
}  // namespace RPCSchemas
