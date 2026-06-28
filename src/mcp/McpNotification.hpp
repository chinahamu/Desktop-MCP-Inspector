#pragma once

#include "McpTypes.hpp"

#include <QJsonObject>
#include <QString>

namespace mcp {

inline constexpr auto kMcpLoggingMessageNotificationMethod = "notifications/message";
inline constexpr auto kMcpLoggingNotificationMethod = "notifications/logging";
inline constexpr auto kMcpProgressNotificationMethod = "notifications/progress";

enum class McpNotificationKind {
  LoggingMessage,
  Progress,
  Other,
};

[[nodiscard]] McpNotificationKind notification_kind(const QString& method);
[[nodiscard]] QString notification_kind_name(McpNotificationKind kind);
[[nodiscard]] QJsonObject notification_params_object(const JsonRpcNotification& notification);
[[nodiscard]] QString notification_summary(const JsonRpcNotification& notification);

} // namespace mcp
