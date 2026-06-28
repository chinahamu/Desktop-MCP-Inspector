#include "McpNotificationTimelineEvent.hpp"

#include "McpNotification.hpp"

namespace timeline {

TimelineEvent mcp_notification_event(const mcp::JsonRpcNotification& notification)
{
  TimelineEvent event;
  event.event_id = make_timeline_event_id();
  event.timestamp = QDateTime::currentDateTimeUtc();
  event.direction = TimelineDirection::Inbound;
  event.kind = TimelineEventKind::Notification;
  event.status = TimelineStatus::Notification;
  event.method = notification.method;
  event.summary = mcp::notification_summary(notification);
  event.payload = QJsonObject{
      {QStringLiteral("notificationKind"), mcp::notification_kind_name(mcp::notification_kind(notification.method))},
      {QStringLiteral("method"), notification.method},
  };

  if (notification.params.has_value()) {
    event.payload.insert(QStringLiteral("params"), *notification.params);
  }

  return event;
}

} // namespace timeline
