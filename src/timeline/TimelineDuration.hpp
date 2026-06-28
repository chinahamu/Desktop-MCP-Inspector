#pragma once

#include "McpClient.hpp"
#include "TimelineEvent.hpp"

#include <QDateTime>

namespace timeline {

[[nodiscard]] TimelineEvent request_event_from_pending_request(
    const mcp::PendingRequest& pending_request,
    QDateTime timestamp = QDateTime::currentDateTimeUtc());
[[nodiscard]] TimelineEvent response_event_from_pending_request(
    const mcp::PendingRequest& pending_request,
    const mcp::JsonRpcResponse& response,
    QDateTime received_at = QDateTime::currentDateTimeUtc());
[[nodiscard]] qint64 duration_ms_between(const QDateTime& started_at, const QDateTime& finished_at);

} // namespace timeline
