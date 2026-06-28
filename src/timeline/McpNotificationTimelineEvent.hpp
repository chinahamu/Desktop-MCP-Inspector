#pragma once

#include "McpTypes.hpp"
#include "TimelineEvent.hpp"

namespace timeline {

[[nodiscard]] TimelineEvent mcp_notification_event(const mcp::JsonRpcNotification& notification);

} // namespace timeline
