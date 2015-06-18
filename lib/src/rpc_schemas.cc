#include <cthun-agent/rpc_schemas.hpp>

#include <cthun-client/protocol/schemas.hpp>

namespace CthunAgent {
namespace RPCSchemas {

// HERE(ale): this must be kept up to date with
// https://github.com/puppetlabs/cthun-specifications

using C_Type = CthunClient::ContentType;
using T_Constraint = CthunClient::TypeConstraint;

CthunClient::Schema BlockingRequestSchema() {
    CthunClient::Schema schema { BLOCKING_REQUEST_TYPE, C_Type::Json };
    // NB: additionalProperties = false
    schema.addConstraint("transaction_id", T_Constraint::String, true);
    schema.addConstraint("module", T_Constraint::String, true);
    schema.addConstraint("action", T_Constraint::String, true);
    schema.addConstraint("params", T_Constraint::Object, false);
    return schema;
}

CthunClient::Schema BlockingResponseSchema() {
    CthunClient::Schema schema { BLOCKING_RESPONSE_TYPE, C_Type::Json };
    // NB: additionalProperties = true
    schema.addConstraint("transaction_id", T_Constraint::String, true);
    return schema;
}

CthunClient::Schema NonBlockingRequestSchema() {
    CthunClient::Schema schema { NON_BLOCKING_REQUEST_TYPE, C_Type::Json };
    // NB: additionalProperties = false
    schema.addConstraint("transaction_id", T_Constraint::String, true);
    schema.addConstraint("notify_outcome", T_Constraint::Bool, true);
    schema.addConstraint("module", T_Constraint::String, true);
    schema.addConstraint("action", T_Constraint::String, true);
    schema.addConstraint("params", T_Constraint::Object, false);
    return schema;
}

CthunClient::Schema NonBlockingResponseSchema() {
    CthunClient::Schema schema { NON_BLOCKING_RESPONSE_TYPE, C_Type::Json };
    // NB: additionalProperties = true
    schema.addConstraint("transaction_id", T_Constraint::String, true);
    schema.addConstraint("job_id", T_Constraint::String, true);
    return schema;
}

CthunClient::Schema ProvisionalResponseSchema() {
    CthunClient::Schema schema { PROVISIONAL_RESPONSE_TYPE, C_Type::Json };
    // NB: additionalProperties = false
    schema.addConstraint("transaction_id", T_Constraint::String, true);
    schema.addConstraint("success", T_Constraint::Bool, true);
    schema.addConstraint("job_id", T_Constraint::String, false);
    schema.addConstraint("error", T_Constraint::String, false);
    return schema;
}

CthunClient::Schema RPCErrorSchema() {
    CthunClient::Schema schema { RPC_ERROR_MSG_TYPE, C_Type::Json };
    // NB: additionalProperties = false
    schema.addConstraint("transaction_id", T_Constraint::String, true);
    schema.addConstraint("id", T_Constraint::String, true);
    schema.addConstraint("description", T_Constraint::String, true);
    return schema;
}

}  // namespace CthunAgent
}  // namespace RPCSchemas
