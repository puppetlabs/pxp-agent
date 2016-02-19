#ifndef SRC_AGENT_ACTION_STATUS_HPP_
#define SRC_AGENT_ACTION_STATUS_HPP_

#include <map>
#include <string>

namespace PXPAgent {

enum class ActionStatus { Unknown, Running, Success, Failure, Undetermined };

static const std::map<ActionStatus, std::string> ACTION_STATUS_NAMES {
    { ActionStatus::Unknown, "unknown" },
    { ActionStatus::Running, "running" },
    { ActionStatus::Success, "success" },
    { ActionStatus::Failure, "failure" },
    { ActionStatus::Undetermined, "undetermined" } };

}  // namespace PXPAgent

#endif  // SRC_AGENT_ACTION_STATUS_HPP_
