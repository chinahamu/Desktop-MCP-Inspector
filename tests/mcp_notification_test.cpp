#include <catch2/catch_test_macros.hpp>

#include "McpNotification.hpp"
#include "McpNotificationTimelineEvent.hpp"

#include <QJsonObject>
#include <QString>

TEST_CASE("MCP logging notifications are summarized", "[mcp][notifications]")
{
  mcp::JsonRpcNotification notification;
  notification.method = QStringLiteral("notifications/message");
  notification.params = QJsonObject{
      {QStringLiteral("level"), QStringLiteral("debug")},
      {QStringLiteral("logger"), QStringLiteral("demo")},
      {QStringLiteral("data"), QStringLiteral("loaded resources")},
  };

  REQUIRE(mcp::notification_kind(notification.method) == mcp::McpNotificationKind::LoggingMessage);
  REQUIRE(mcp::notification_kind_name(mcp::notification_kind(notification.method)) == QStringLiteral("logging"));
  REQUIRE(mcp::notification_summary(notification).contains(QStringLiteral("loaded resources")));
}

TEST_CASE("MCP progress notifications are summarized", "[mcp][notifications]")
{
  mcp::JsonRpcNotification notification;
  notification.method = QStringLiteral("notifications/progress");
  notification.params = QJsonObject{
      {QStringLiteral("progressToken"), QStringLiteral("scan")},
      {QStringLiteral("progress"), 2},
      {QStringLiteral("total"), 5},
      {QStringLiteral("message"), QStringLiteral("Reading resources")},
  };

  REQUIRE(mcp::notification_kind(notification.method) == mcp::McpNotificationKind::Progress);
  REQUIRE(mcp::notification_summary(notification).contains(QStringLiteral("2/5")));
  REQUIRE(mcp::notification_summary(notification).contains(QStringLiteral("Reading resources")));
}

TEST_CASE("MCP notifications become timeline notification events", "[mcp][notifications][timeline]")
{
  mcp::JsonRpcNotification notification;
  notification.method = QStringLiteral("notifications/progress");
  notification.params = QJsonObject{
      {QStringLiteral("progress"), 1},
      {QStringLiteral("total"), 1},
  };

  const auto event = timeline::mcp_notification_event(notification);

  REQUIRE(event.kind == timeline::TimelineEventKind::Notification);
  REQUIRE(event.status == timeline::TimelineStatus::Notification);
  REQUIRE(event.direction == timeline::TimelineDirection::Inbound);
  REQUIRE(event.method == QStringLiteral("notifications/progress"));
  REQUIRE(event.payload.value(QStringLiteral("notificationKind")).toString() == QStringLiteral("progress"));
}
