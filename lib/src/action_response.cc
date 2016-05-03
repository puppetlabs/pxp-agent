#include <pxp-agent/action_response.hpp>

#include <cpp-pcp-client/validator/validator.hpp>
#include <cpp-pcp-client/validator/schema.hpp>

#include <leatherman/util/time.hpp>

#include <leatherman/locale/locale.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.action_response"
#include <leatherman/logging/logging.hpp>

#include <cassert>
#include <utility>  // std::forward

namespace PXPAgent {

namespace lth_jc   = leatherman::json_container;
namespace lth_util = leatherman::util;
namespace lth_loc  = leatherman::locale;
using R_T = ActionResponse::ResponseType;

const std::string ACTION_METADATA_SCHEMA { "action_metdata_schema" };

const std::string REQUESTER { "requester" };
const std::string MODULE { "module" };
const std::string ACTION { "action" };
const std::string REQUEST_PARAMS { "request_params" };
const std::string TRANSACTION_ID { "transaction_id" };
const std::string REQUEST_ID { "request_id" };
const std::string NOTIFY_OUTCOME { "notify_outcome" };
const std::string START { "start" };
const std::string STATUS { "status" };
const std::string END { "end" };
const std::string RESULTS { "results" };
const std::string RESULTS_ARE_VALID { "results_are_valid" };
const std::string EXECUTION_ERROR { "execution_error" };

static PCPClient::Validator getActionMetadataValidator()
{
    using T_C = PCPClient::TypeConstraint;
    PCPClient::Schema sch { ACTION_METADATA_SCHEMA, PCPClient::ContentType::Json };

    // Entries created during initialization (all mandatory)
    sch.addConstraint(REQUESTER, T_C::String, true);
    sch.addConstraint(MODULE, T_C::String, true);
    sch.addConstraint(ACTION, T_C::String, true);
    sch.addConstraint(REQUEST_PARAMS, T_C::String, true);
    sch.addConstraint(TRANSACTION_ID, T_C::String, true);
    sch.addConstraint(REQUEST_ID, T_C::String, true);
    sch.addConstraint(NOTIFY_OUTCOME, T_C::Bool, true);
    sch.addConstraint(START, T_C::String, true);
    sch.addConstraint(STATUS, T_C::String, true);

    // Entries created after processing the action's output
    sch.addConstraint(END, T_C::String, false);
    sch.addConstraint(RESULTS, T_C::Any, false);
    sch.addConstraint(RESULTS_ARE_VALID, T_C::Bool, false);
    sch.addConstraint(EXECUTION_ERROR, T_C::String, false);

    PCPClient::Validator validator {};
    validator.registerSchema(sch);
    return validator;
}

//
// Static member functions
//

lth_jc::JsonContainer
ActionResponse::getMetadataFromRequest(const ActionRequest& request)
{
    lth_jc::JsonContainer m {};
    m.set<std::string>(REQUESTER, request.sender());
    m.set<std::string>(MODULE, request.module());
    m.set<std::string>(ACTION, request.action());
    m.set<std::string>(REQUEST_PARAMS,
        request.paramsTxt().empty() ? "none" : request.paramsTxt());

    m.set<std::string>(TRANSACTION_ID, request.transactionId());
    m.set<std::string>(REQUEST_ID, request.id());
    m.set<bool>(NOTIFY_OUTCOME, request.notifyOutcome());
    m.set<std::string>(START, lth_util::get_ISO8601_time());
    m.set<std::string>(STATUS, ACTION_STATUS_NAMES.at(ActionStatus::Running));

    return m;
}

//
// Public interface
//

ActionResponse::ActionResponse(ModuleType module_type_,
                               const ActionRequest& request,
                               std::string status_query_transaction_)
        : module_type { module_type_ },
          request_type { request.type() },
          output {},
          action_metadata { getMetadataFromRequest(request) },
          status_query_transaction { status_query_transaction_ }
{
}

ActionResponse::ActionResponse(ModuleType m_t,
                               RequestType r_t,
                               ActionOutput out,
                               lth_jc::JsonContainer&& a_m)
        : module_type(m_t),
          request_type(r_t),
          output(out),
          action_metadata(std::forward<lth_jc::JsonContainer>(a_m)),
          status_query_transaction()
{
    if (!valid())
        throw Error { lth_loc::translate("invalid action metadata") };
}

void ActionResponse::setStatus(ActionStatus status)
{
    action_metadata.set<std::string>(STATUS, ACTION_STATUS_NAMES.at(status));
}

void ActionResponse::setValidResultsAndEnd(lth_jc::JsonContainer&& results,
                                           const std::string& execution_error)
{
    action_metadata.set<std::string>(END, lth_util::get_ISO8601_time());
    action_metadata.set<bool>(RESULTS_ARE_VALID, true);
    action_metadata.set<lth_jc::JsonContainer>(RESULTS,
        std::forward<lth_jc::JsonContainer>(results));
    action_metadata.set<std::string>(STATUS,
        ACTION_STATUS_NAMES.at(ActionStatus::Success));
    if (!execution_error.empty())
        action_metadata.set<std::string>(EXECUTION_ERROR, execution_error);
}

void ActionResponse::setBadResultsAndEnd(const std::string& execution_error)
{
    action_metadata.set<std::string>(END, lth_util::get_ISO8601_time());
    action_metadata.set<bool>(RESULTS_ARE_VALID, false);
    action_metadata.set<std::string>(EXECUTION_ERROR, execution_error);
    action_metadata.set<std::string>(STATUS,
        ACTION_STATUS_NAMES.at(ActionStatus::Failure));
}

const std::string& ActionResponse::prettyRequestLabel() const
{
    if (pretty_request_label_.empty())
        pretty_request_label_ =
            lth_loc::format("{1} '{2} {3}' request (transaction {4})",
                            REQUEST_TYPE_NAMES.at(request_type),
                            action_metadata.get<std::string>(MODULE),
                            action_metadata.get<std::string>(ACTION),
                            action_metadata.get<std::string>(TRANSACTION_ID));

    return pretty_request_label_;
}

bool ActionResponse::isValidActionMetadata(const lth_jc::JsonContainer& metadata)
{
    static PCPClient::Validator validator { getActionMetadataValidator() };
    try {
        validator.validate(metadata, ACTION_METADATA_SCHEMA);
        return true;
    } catch (const PCPClient::validation_error& e) {
        LOG_TRACE("Invalid action metadata: {1}", e.what());
    }
    return false;
}


bool ActionResponse::valid() const
{
    return ActionResponse::isValidActionMetadata(action_metadata);
}

bool ActionResponse::valid(R_T response_type) const
{
    if (!valid())
        return false;

    bool is_valid { false };

    switch (response_type) {
        case (R_T::Blocking):
        case (R_T::NonBlocking):
            is_valid = action_metadata.includes(RESULTS);
            break;
        case (R_T::StatusOutput):
            if (action_metadata.includes("status")) {
                auto found = NAMES_OF_ACTION_STATUS.find(
                    action_metadata.get<std::string>("status"));
                if (found != NAMES_OF_ACTION_STATUS.end())
                    is_valid = !status_query_transaction.empty();
            }

            break;
        case (R_T::RPCError):
            is_valid = action_metadata.includes(EXECUTION_ERROR);
    }

    return is_valid;
}

// TODO(ale): update this after PXP v2.0 changes
lth_jc::JsonContainer ActionResponse::toJSON(R_T response_type) const
{
    lth_jc::JsonContainer r {};
    r.set<std::string>(TRANSACTION_ID,
        action_metadata.get<std::string>(TRANSACTION_ID));

    switch (response_type) {
        case (R_T::Blocking):
        case (R_T::NonBlocking):
            r.set<lth_jc::JsonContainer>(RESULTS,
                action_metadata.get<lth_jc::JsonContainer>(RESULTS));
            break;
        case (R_T::StatusOutput):
        {
            auto action_status = action_metadata.get<std::string>({ RESULTS, STATUS });
            lth_jc::JsonContainer action_results {};
            action_results.set<std::string>(TRANSACTION_ID, status_query_transaction);

            if (action_status == ACTION_STATUS_NAMES.at(ActionStatus::Running)) {
                action_results.set<std::string>(STATUS,
                    ACTION_STATUS_NAMES.at(ActionStatus::Running));
            } else if (action_status == ACTION_STATUS_NAMES.at(ActionStatus::Success)
                    || action_status == ACTION_STATUS_NAMES.at(ActionStatus::Failure)) {
                // TODO(ale): decouple the status of the action from
                // the output of the action once PXP v.2 gets in, as
                // doing so would break compatibility against old
                // clj-pxp-puppet; in practice set STATUS to
                // action_status, instead of setting it to "failure"
                // (in case the output is bad) or, otherwise, to
                // (exitcode == EXIT_SUCCESS), as done in:
                // https://github.com/puppetlabs/pxp-agent/blob/1.0.2/lib/src/modules/status.cc#L232
                action_results.set<int>("exitcode", output.exitcode);

                if (action_status == ACTION_STATUS_NAMES.at(ActionStatus::Failure)) {
                    // The output was bad; report a failure
                    action_results.set<std::string>(STATUS,
                        ACTION_STATUS_NAMES.at(ActionStatus::Failure));
                } else {
                    // The output was good; use exitcode
                    action_results.set<std::string>(STATUS,
                        (output.exitcode == EXIT_SUCCESS
                            ? ACTION_STATUS_NAMES.at(ActionStatus::Success)
                            : ACTION_STATUS_NAMES.at(ActionStatus::Failure)));
                }
            } else {
                // TODO(ale): also UNDETERMINED once PXP v.2 is in
                action_results.set<std::string>(STATUS,
                    ACTION_STATUS_NAMES.at(ActionStatus::Unknown));
            }

            if (!output.std_out.empty())
                action_results.set<std::string>("stdout", output.std_out);
            if (!output.std_err.empty())
                action_results.set<std::string>("stderr", output.std_err);
            r.set<lth_jc::JsonContainer>(RESULTS, action_results);

            break;
        }
        case (R_T::RPCError):
            r.set<std::string>("id", action_metadata.get<std::string>(REQUEST_ID));
            r.set<std::string>("description",
                action_metadata.get<std::string>(EXECUTION_ERROR));
            break;
    }

    return r;
}

}  // namespace PXPAgent
