#ifndef SRC_AGENT_ACTION_REQUEST_HPP_
#define SRC_AGENT_ACTION_REQUEST_HPP_

#include <pxp-agent/request_type.hpp>

#include <leatherman/json_container/json_container.hpp>

#include <stdexcept>
#include <string>
#include <map>
#include <vector>

namespace PXPAgent {

class ActionRequest {
  public:
    ActionRequest(RequestType type_,
                  std::string id_,
                  std::string sender_,
                  leatherman::json_container::JsonContainer data_,
                  std::vector<leatherman::json_container::JsonContainer> debug_ = {});

    void setResultsDir(const std::string& results_dir) const;

    const RequestType& type() const;
    const std::string& id() const;
    const std::string& sender() const;
    const std::string& transactionId() const;
    const std::string& module() const;
    const std::string& action() const;
    const bool& notifyOutcome() const;
    const std::string& resultsDir() const;
    const std::vector<leatherman::json_container::JsonContainer>& debug() const;

    // The following accessors perform lazy initialization
    // The params entry is not required; in case it's not included
    // in the request, an empty JsonContainer object is returned
    const leatherman::json_container::JsonContainer& params() const;
    const std::string& paramsTxt() const;
    const std::string& prettyLabel() const;

  private:
    RequestType type_;
    std::string id_;
    std::string sender_;
    std::string transaction_id_;
    std::string module_;
    std::string action_;
    bool notify_outcome_;
    leatherman::json_container::JsonContainer data_;
    std::vector<leatherman::json_container::JsonContainer> debug_;

    // Lazy initialized; no setter is available
    mutable leatherman::json_container::JsonContainer params_;
    mutable std::string params_txt_;
    mutable std::string pretty_label_;

    // This has its own setter - it's not part of request's state
    mutable std::string results_dir_;
};

}  // namespace PXPAgent

#endif  // SRC_AGENT_ACTION_REQUEST_HPP_
