#include <cthun-agent/action_request.hpp>
#include <cthun-agent/errors.hpp>
#include <cthun-agent/configuration.hpp>

#define LEATHERMAN_LOGGING_NAMESPACE "puppetlabs.cthun_agent.action_request"
#include <leatherman/logging/logging.hpp>

#include <cassert>

namespace CthunAgent {

ActionRequest::ActionRequest(RequestType type,
                             const CthunClient::ParsedChunks& parsed_chunks)
        : type_ { type },
          notify_outcome_ { true },
          parsed_chunks_ { parsed_chunks },
          params_ { "{}" },
          config_ { "{}" },
          request_txt_ { "" },
          params_txt_ { "" } {
    init();
}

ActionRequest::ActionRequest(RequestType type,
                             CthunClient::ParsedChunks&& parsed_chunks)
        : type_ { type },
          notify_outcome_ { true },
          parsed_chunks_ { parsed_chunks },
          params_ { "{}" },
          config_ { "{}" },
          request_txt_ { "" },
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

const CthunClient::ParsedChunks& ActionRequest::parsedChunks() const {
    return parsed_chunks_;
}

const lth_jc::JsonContainer& ActionRequest::params() const {
    if (params_.empty()) {
        params_ = parsed_chunks_.data.get<lth_jc::JsonContainer>("params");
    }
    return params_;
}

const lth_jc::JsonContainer& ActionRequest::config() const {
    if (config_.empty()) {
        config_ = Configuration::Instance().getModuleConfig(module_);
    }
    return config_;
}

const std::string& ActionRequest::paramsTxt() const {
    if (params_txt_.empty()) {
        params_txt_ = params().toString();
    }
    return params_txt_;
}

const std::string& ActionRequest::requestTxt() const {
    if (request_txt_.empty()) {
        lth_jc::JsonContainer tmp {};
        tmp.set<lth_jc::JsonContainer>("params", params());
        tmp.set<lth_jc::JsonContainer>("config", config());
        request_txt_ = tmp.toString();
    }
    return request_txt_;
}

// Private interface

void ActionRequest::init() {
    id_ = parsed_chunks_.envelope.get<std::string>("id");
    sender_ = parsed_chunks_.envelope.get<std::string>("sender");

    LOG_INFO("Validating %1% request %2% by %3%",
             requestTypeNames[type_], id_, sender_);
    LOG_DEBUG("Request %1%:\n%2%", id_, parsed_chunks_.toString());

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
    if (parsed_chunks_.data_type != CthunClient::ContentType::Json) {
        throw ActionRequest::Error { "data is not in JSON format" };
    }
}

}  // namespace CthunAgent
