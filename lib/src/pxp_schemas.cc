#include <pxp-agent/pxp_schemas.hpp>

namespace PXPAgent {
namespace PXPSchemas {

// HERE(ale): this must be kept up to date with
// https://github.com/puppetlabs/pcp-specifications

namespace lth_jc = leatherman::json_container;
using C_Type = lth_jc::ContentType;
using T_Constraint = lth_jc::TypeConstraint;

lth_jc::Schema BlockingRequestSchema() {
    lth_jc::Schema schema { BLOCKING_REQUEST_TYPE, C_Type::Json };
    // NB: additionalProperties = false
    schema.addConstraint("transaction_id", T_Constraint::String, true);
    schema.addConstraint("module", T_Constraint::String, true);
    schema.addConstraint("action", T_Constraint::String, true);
    schema.addConstraint("params", T_Constraint::Object, false);
    return schema;
}

lth_jc::Schema BlockingResponseSchema() {
    lth_jc::Schema schema { BLOCKING_RESPONSE_TYPE, C_Type::Json };
    // NB: additionalProperties = false
    schema.addConstraint("transaction_id", T_Constraint::String, true);
    schema.addConstraint("results", T_Constraint::Object, true);
    return schema;
}

lth_jc::Schema NonBlockingRequestSchema() {
    lth_jc::Schema schema { NON_BLOCKING_REQUEST_TYPE, C_Type::Json };
    // NB: additionalProperties = false
    schema.addConstraint("transaction_id", T_Constraint::String, true);
    schema.addConstraint("notify_outcome", T_Constraint::Bool, true);
    schema.addConstraint("module", T_Constraint::String, true);
    schema.addConstraint("action", T_Constraint::String, true);
    schema.addConstraint("params", T_Constraint::Object, false);
    return schema;
}

lth_jc::Schema NonBlockingResponseSchema() {
    lth_jc::Schema schema { NON_BLOCKING_RESPONSE_TYPE, C_Type::Json };
    // NB: additionalProperties = false
    schema.addConstraint("transaction_id", T_Constraint::String, true);
    schema.addConstraint("results", T_Constraint::Object, true);
    return schema;
}

lth_jc::Schema ProvisionalResponseSchema() {
    lth_jc::Schema schema { PROVISIONAL_RESPONSE_TYPE, C_Type::Json };
    // NB: additionalProperties = false
    schema.addConstraint("transaction_id", T_Constraint::String, true);
    return schema;
}

lth_jc::Schema PXPErrorSchema() {
    lth_jc::Schema schema { PXP_ERROR_MSG_TYPE, C_Type::Json };
    // NB: additionalProperties = false
    schema.addConstraint("transaction_id", T_Constraint::String, true);
    schema.addConstraint("id", T_Constraint::String, true);
    schema.addConstraint("description", T_Constraint::String, true);
    return schema;
}

}  // namespace PXPAgent
}  // namespace PXPSchemas
