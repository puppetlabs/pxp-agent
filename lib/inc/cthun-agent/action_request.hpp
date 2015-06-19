#ifndef SRC_AGENT_ACTION_REQUEST_HPP_
#define SRC_AGENT_ACTION_REQUEST_HPP_

#include <cthun-client/protocol/chunks.hpp>      // ParsedChunk

#include <stdexcept>
#include <string>
#include <map>

namespace CthunAgent {

enum class RequestType { Blocking, NonBlocking };
static std::map<RequestType, std::string> requestTypeNames {
    { RequestType::Blocking, "blocking" },
    { RequestType::NonBlocking, "non blocking" } };

class ActionRequest {
  public:
    ActionRequest();

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
    const CthunClient::ParsedChunks& parsedChunks() const;

  private:
    RequestType type_;
    std::string id_;
    std::string sender_;
    std::string transaction_id_;
    std::string module_;
    std::string action_;
    CthunClient::ParsedChunks parsed_chunks_;

    void
    init();
    void validateFormat();
};

}  // namespace CthunAgent

#endif  // SRC_AGENT_ACTION_REQUEST_HPP_
