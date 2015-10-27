#ifndef SRC_AGENT_ACTION_REQUEST_HPP_
#define SRC_AGENT_ACTION_REQUEST_HPP_

#include <cpp-pcp-client/protocol/chunks.hpp>      // ParsedChunk

#include <leatherman/json_container/json_container.hpp>

#include <pxp-agent/export.h>

#include <stdexcept>
#include <string>
#include <map>

namespace PXPAgent {

namespace lth_jc = leatherman::json_container;

enum class RequestType { Blocking, NonBlocking };
static std::map<RequestType, std::string> requestTypeNames {
    { RequestType::Blocking, "blocking" },
    { RequestType::NonBlocking, "non blocking" } };

class LIBPXP_AGENT_EXPORT ActionRequest {
  public:
    struct Error : public std::runtime_error {
        explicit Error(std::string const& msg) : std::runtime_error(msg) {}
    };

    /// Throws an ActionRequest::Error in case it fails to retrieve
    /// the data chunk from the specified ParsedChunks or in case of
    /// binary data (currently not supported).
    ActionRequest(RequestType type_,
                  const PCPClient::ParsedChunks& parsed_chunks_);
    ActionRequest(RequestType type_,
                  PCPClient::ParsedChunks&& parsed_chunks_);

    const RequestType& type() const;
    const std::string& id() const;
    const std::string& sender() const;
    const std::string& transactionId() const;
    const std::string& module() const;
    const std::string& action() const;
    const bool& notifyOutcome() const;
    const PCPClient::ParsedChunks& parsedChunks() const;

    // The following accessors perform lazy initialization
    // The params entry is not required; in case it's not included
    // in the request, an empty JsonContainer object is returned
    const lth_jc::JsonContainer& params() const;
    const std::string& paramsTxt() const;

  private:
    RequestType type_;
    std::string id_;
    std::string sender_;
    std::string transaction_id_;
    std::string module_;
    std::string action_;
    bool notify_outcome_;
    PCPClient::ParsedChunks parsed_chunks_;

    // Lazy initialized
    mutable lth_jc::JsonContainer params_;
    mutable std::string params_txt_;

    void init();
    void validateFormat();
};

}  // namespace PXPAgent

#endif  // SRC_AGENT_ACTION_REQUEST_HPP_
