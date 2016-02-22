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

static const std::map<std::string, ActionStatus> NAMES_OF_ACTION_STATUS {
    { "unknown", ActionStatus::Unknown },
    { "running", ActionStatus::Running },
    { "success", ActionStatus::Success },
    { "failure", ActionStatus::Failure },
    { "undetermined", ActionStatus::Undetermined } };

}  // namespace PXPAgent

#endif  // SRC_AGENT_ACTION_STATUS_HPP_
