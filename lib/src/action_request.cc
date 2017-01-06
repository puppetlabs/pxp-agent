#include <pxp-agent/action_request.hpp>

#include <leatherman/locale/locale.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.action_request"
#include <leatherman/logging/logging.hpp>

namespace PXPAgent {

namespace lth_jc  = leatherman::json_container;
namespace lth_loc = leatherman::locale;

ActionRequest::ActionRequest(RequestType type,
                             std::string message_id,
                             std::string sender,
                             lth_jc::JsonContainer data,
                             std::vector<lth_jc::JsonContainer> debug)
        : type_ { type },
          id_ { std::move(message_id) },
          sender_ { std::move(sender) },
          transaction_id_ { data.get<std::string>("transaction_id") },
          module_ { data.get<std::string>("module") },
          action_ { data.get<std::string>("action") },
          notify_outcome_ { type == RequestType::NonBlocking ?
              data.get<bool>("notify_outcome") : true },
          data_ { std::move(data) },
          debug_ { std::move(debug) },
          params_ { "{}" },
          params_txt_ {},
          pretty_label_ {},
          results_dir_ {} {
}

void ActionRequest::setResultsDir(const std::string& results_dir) const {
    results_dir_ = results_dir;
}

const RequestType& ActionRequest::type() const { return type_; }
const std::string& ActionRequest::id() const { return id_; }
const std::string& ActionRequest::sender() const{ return sender_; }
const std::string& ActionRequest::transactionId() const { return transaction_id_; }
const std::string& ActionRequest::module() const { return module_; }
const std::string& ActionRequest::action() const { return action_; }
const bool& ActionRequest::notifyOutcome() const { return notify_outcome_; }

const std::string& ActionRequest::resultsDir() const { return results_dir_; }

const std::vector<leatherman::json_container::JsonContainer>&
ActionRequest::debug() const { return debug_; }

const lth_jc::JsonContainer& ActionRequest::params() const {
    if (params_.empty() && data_.includes("params"))
        params_ = data_.get<lth_jc::JsonContainer>("params");

    return params_;
}

const std::string& ActionRequest::paramsTxt() const {
    if (params_txt_.empty())
        params_txt_ = params().toString();

    return params_txt_;
}

const std::string& ActionRequest::prettyLabel() const {
    if (pretty_label_.empty())
        pretty_label_ = lth_loc::format("{1} '{2} {3}' request (transaction {4})",
                                        REQUEST_TYPE_NAMES.at(type_), module_,
                                        action_, transaction_id_);

    return pretty_label_;
}

}  // namespace PXPAgent
