#include <pxp-agent/action_request.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.pxp_agent.action_request"
#include <leatherman/logging/logging.hpp>

#include <cassert>

namespace PXPAgent {

ActionRequest::ActionRequest(RequestType type,
                             const PCPClient::ParsedChunks& parsed_chunks)
        : type_ { type },
          notify_outcome_ { true },
          parsed_chunks_ { parsed_chunks },
          params_ { "{}" },
          params_txt_ { "" } {
    init();
}

ActionRequest::ActionRequest(RequestType type,
                             PCPClient::ParsedChunks&& parsed_chunks)
        : type_ { type },
          notify_outcome_ { true },
          parsed_chunks_ { parsed_chunks },
          params_ { "{}" },
          params_txt_ { "" } {
    init();
}

const RequestType& ActionRequest::type() const { return type_; }
const std::string& ActionRequest::id() const { return id_; }
const std::string& ActionRequest::sender() const{ return sender_; }
const std::string& ActionRequest::transactionId() const { return transaction_id_; }
const std::string& ActionRequest::module() const { return module_; }
const std::string& ActionRequest::action() const { return action_; }
const bool& ActionRequest::notifyOutcome() const { return notify_outcome_; }

const PCPClient::ParsedChunks& ActionRequest::parsedChunks() const {
    return parsed_chunks_;
}

const lth_jc::JsonContainer& ActionRequest::params() const {
    if (params_.empty() && parsed_chunks_.data.includes("params")) {
        params_ = parsed_chunks_.data.get<lth_jc::JsonContainer>("params");
    }
    return params_;
}

const std::string& ActionRequest::paramsTxt() const {
    if (params_txt_.empty()) {
        params_txt_ = params().toString();
    }
    return params_txt_;
}

// Private interface

void ActionRequest::init() {
    id_ = parsed_chunks_.envelope.get<std::string>("id");
    sender_ = parsed_chunks_.envelope.get<std::string>("sender");

    LOG_DEBUG("Validating %1% request %2% by %3%:\n%4%",
              requestTypeNames[type_], id_, sender_, parsed_chunks_.toString());

    validateFormat();

    transaction_id_ = parsed_chunks_.data.get<std::string>("transaction_id");
    module_ = parsed_chunks_.data.get<std::string>("module");
    action_ = parsed_chunks_.data.get<std::string>("action");

    if (type_ == RequestType::NonBlocking) {
        notify_outcome_ = parsed_chunks_.data.get<bool>("notify_outcome");
    }
}

void ActionRequest::validateFormat() {
    if (!parsed_chunks_.has_data) {
        throw ActionRequest::Error { "no data" };
    }
    if (parsed_chunks_.invalid_data) {
        throw ActionRequest::Error { "invalid data" };
    }
    // NOTE(ale): currently, we don't support ContentType::Binary
    if (parsed_chunks_.data_type != PCPClient::ContentType::Json) {
        throw ActionRequest::Error { "data is not in JSON format" };
    }
}

}  // namespace PXPAgent
