#ifndef SRC_AGENT_ACTION_REQUEST_HPP_
#define SRC_AGENT_ACTION_REQUEST_HPP_

#include <pxp-agent/request_type.hpp>

#include <cpp-pcp-client/protocol/chunks.hpp>      // ParsedChunk

#include <leatherman/json_container/json_container.hpp>

#include <stdexcept>
#include <string>
#include <map>

namespace PXPAgent {

class ActionRequest {
  public:
    struct Error : public std::runtime_error {
        explicit Error(std::string const& msg) : std::runtime_error(msg) {}
    };

    /// Throws an ActionRequest::Error in case it fails to retrieve
    /// the data chunk from the specified ParsedChunks or in case of
    /// binary data (currently not supported).
    ActionRequest(RequestType type_,
                  PCPClient::ParsedChunks parsed_chunks_);

    void setResultsDir(const std::string& results_dir) const;

    const RequestType& type() const;
    const std::string& id() const;
    const std::string& sender() const;
    const std::string& transactionId() const;
    const std::string& module() const;
    const std::string& action() const;
    const bool& notifyOutcome() const;
    const PCPClient::ParsedChunks& parsedChunks() const;
    const std::string& resultsDir() const;

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
    PCPClient::ParsedChunks parsed_chunks_;

    // Lazy initialized; no setter is available
    mutable leatherman::json_container::JsonContainer params_;
    mutable std::string params_txt_;
    mutable std::string pretty_label_;

    // This has its own setter - it's not part of request's state
    mutable std::string results_dir_;

    void init();
    void validateFormat();
};

}  // namespace PXPAgent

#endif  // SRC_AGENT_ACTION_REQUEST_HPP_
