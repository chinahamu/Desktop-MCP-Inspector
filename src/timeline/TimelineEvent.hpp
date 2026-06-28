#pragma once

#include "McpTypes.hpp"

#include <QDateTime>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QtTypes>

#include <optional>

namespace timeline {

enum class TimelineDirection {
  Inbound,
  Outbound,
  Internal,
};

enum class TimelineEventKind {
  Request,
  Response,
  Notification,
  Stderr,
  TransportError,
  Lifecycle,
};

enum class TimelineStatus {
  Pending,
  Success,
  Error,
  Notification,
  Log,
  Cancelled,
  Timeout,
};

struct TimelineEvent
{
  QString event_id;
  QDateTime timestamp;
  TimelineDirection direction = TimelineDirection::Internal;
  TimelineEventKind kind = TimelineEventKind::Lifecycle;
  TimelineStatus status = TimelineStatus::Success;
  std::optional<mcp::RequestId> request_id;
  QString method;
  QString summary;
  QJsonObject payload;
  std::optional<qint64> duration_ms;

  friend bool operator==(const TimelineEvent& lhs, const TimelineEvent& rhs) = default;
};

[[nodiscard]] QString make_timeline_event_id();
[[nodiscard]] QString timeline_direction_name(TimelineDirection direction);
[[nodiscard]] QString timeline_event_kind_name(TimelineEventKind kind);
[[nodiscard]] QString timeline_status_name(TimelineStatus status);
[[nodiscard]] QString request_id_to_display(const mcp::RequestId& request_id);
[[nodiscard]] QJsonValue request_id_to_json_value(const mcp::RequestId& request_id);
[[nodiscard]] QJsonObject to_json_object(const TimelineEvent& event);

} // namespace timeline
