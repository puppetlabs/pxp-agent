#ifndef SRC_AGENT_REQUEST_TYPE_HPP_
#define SRC_AGENT_REQUEST_TYPE_HPP_

#include <map>
#include <string>

namespace PXPAgent {

enum class RequestType { Blocking, NonBlocking };

static const std::map<RequestType, std::string> REQUEST_TYPE_NAMES {
    { RequestType::Blocking, "blocking" },
    { RequestType::NonBlocking, "non blocking" } };

}  // namespace PXPAgent

#endif  // SRC_AGENT_REQUEST_TYPE_HPP_
