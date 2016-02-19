#ifndef SRC_AGENT_ACTION_RESPONSE_HPP
#define SRC_AGENT_ACTION_RESPONSE_HPP

#include <pxp-agent/action_output.hpp>
#include <pxp-agent/action_request.hpp>
#include <pxp-agent/action_status.hpp>
#include <pxp-agent/module_type.hpp>
#include <pxp-agent/request_type.hpp>

#include <leatherman/json_container/json_container.hpp>

#include <string>
#include <stdexcept>

namespace PXPAgent {

class ActionResponse {
  public:
    struct Error : public std::runtime_error {
        explicit Error(std::string const& msg) : std::runtime_error(msg) {}
    };

    enum class ResponseType { Blocking, NonBlocking, StatusOutput, RPCError };

    ModuleType module_type;
    RequestType request_type;
    ActionOutput output;
    leatherman::json_container::JsonContainer action_metadata;

    static leatherman::json_container::JsonContainer
    getMetadataFromRequest(const ActionRequest& request);

    ActionResponse(ModuleType module_type_,
                   const ActionRequest& request_);

    // Throws an Error if action_metadata is invalid.
    ActionResponse(ModuleType module_type_,
                   RequestType request_type_,
                   ActionOutput output_,
                   leatherman::json_container::JsonContainer&& action_metadata_);

    void setStatus(ActionStatus status);

    // Set results_are_valid to true in the action metadata and update
    // the specified entries.
    void setValidResultsAndEnd(leatherman::json_container::JsonContainer&& results,
                               const std::string& execution_error = "");

    // Set results_are_valid to false in the action metadata and
    // update the execution_error entry.
    void setBadResultsAndEnd(const std::string& execution_error);

    const std::string& prettyRequestLabel() const;

    // Returns true if specified JSON object is conform to the
    // action_metadata schema.
    static bool isValidActionMetadata(
        const leatherman::json_container::JsonContainer & metadata);

    // Returns true if action_metadata has all the required entries
    // (the entries created at initialization).
    bool valid() const;

    // Returns true if action_metadata has all the required entries
    // (the entries created at initialization) plus the entries
    // required to send a message of the specified type.
    bool valid(ResponseType response_type) const;

    // Returns a JSON object suitable to be used in the data content
    // of a PXP message of the specified response type.
    // Throws a PCPClient::JsonContainer::data_key_error in case a
    // required entry is missing.
    leatherman::json_container::JsonContainer
    toJSON(ResponseType response_type) const;

  private:
    mutable std::string pretty_request_label_;
};

}  // namespace PXPAgents

#endif  // SRC_AGENT_ACTION_RESPONSE_HPP
