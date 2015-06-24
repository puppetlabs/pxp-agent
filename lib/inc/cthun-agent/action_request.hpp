#ifndef SRC_AGENT_ACTION_REQUEST_HPP_
#define SRC_AGENT_ACTION_REQUEST_HPP_

#include <cthun-client/protocol/chunks.hpp>      // ParsedChunk

#include <leatherman/json_container/json_container.hpp>

#include <stdexcept>
#include <string>
#include <map>

namespace CthunAgent {

namespace LTH_JC = leatherman::json_container;

enum class RequestType { Blocking, NonBlocking };
static std::map<RequestType, std::string> requestTypeNames {
    { RequestType::Blocking, "blocking" },
    { RequestType::NonBlocking, "non blocking" } };

class ActionRequest {
  public:
    /// Throw a request_format_error in case is not possible to
    /// retrieve the data chunk.
    ActionRequest(RequestType type_,
                  const CthunClient::ParsedChunks& parsed_chunks_);
    ActionRequest(RequestType type_,
                  CthunClient::ParsedChunks&& parsed_chunks_);

    const RequestType& type() const;
    const std::string& id() const;
    const std::string& sender() const;
    const std::string& transactionId() const;
    const std::string& module() const;
    const std::string& action() const;
    const bool& notifyOutcome() const;
    const CthunClient::ParsedChunks& parsedChunks() const;

    // The following accessors perform lazy initialization
    const LTH_JC::JsonContainer& params() const;
    const std::string& paramsTxt() const;

  private:
    RequestType type_;
    std::string id_;
    std::string sender_;
    std::string transaction_id_;
    std::string module_;
    std::string action_;
    bool notify_outcome_;
    CthunClient::ParsedChunks parsed_chunks_;

    // Lazy initialized
    mutable LTH_JC::JsonContainer params_;
    mutable std::string params_txt_;

    void init();
    void validateFormat();
};

}  // namespace CthunAgent

#endif  // SRC_AGENT_ACTION_REQUEST_HPP_
