#include "McpNotification.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QStringList>

namespace mcp {
namespace {

[[nodiscard]] QString compact_json(const QJsonObject& object)
{
  return QString::fromUtf8(QJsonDocument{object}.toJson(QJsonDocument::Compact));
}

[[nodiscard]] QString compact_json(const QJsonArray& array)
{
  return QString::fromUtf8(QJsonDocument{array}.toJson(QJsonDocument::Compact));
}

[[nodiscard]] QString json_value_to_display(const QJsonValue& value)
{
  if (value.isString()) {
    return value.toString();
  }
  if (value.isBool()) {
    return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
  }
  if (value.isDouble()) {
    return QString::number(value.toDouble());
  }
  if (value.isObject()) {
    return compact_json(value.toObject());
  }
  if (value.isArray()) {
    return compact_json(value.toArray());
  }
  if (value.isNull()) {
    return QStringLiteral("null");
  }
  return QString{};
}

[[nodiscard]] QString logging_summary(const QString& method, const QJsonObject& params)
{
  const auto level = params.value(QStringLiteral("level")).toString(QStringLiteral("log"));
  const auto logger = params.value(QStringLiteral("logger")).toString();
  const auto data = json_value_to_display(params.value(QStringLiteral("data")));
  const auto message = data.isEmpty() ? params.value(QStringLiteral("message")).toString() : data;

  QString prefix = level;
  if (!logger.isEmpty()) {
    prefix = QStringLiteral("%1/%2").arg(level, logger);
  }

  if (!message.isEmpty()) {
    return QStringLiteral("%1: %2").arg(prefix, message);
  }

  return QStringLiteral("%1 notification: %2").arg(method, compact_json(params));
}

[[nodiscard]] QString progress_summary(const QString& method, const QJsonObject& params)
{
  const auto marker = json_value_to_display(params.value(QStringLiteral("progressToken")));
  const auto progress = json_value_to_display(params.value(QStringLiteral("progress")));
  const auto total = json_value_to_display(params.value(QStringLiteral("total")));
  const auto message = params.value(QStringLiteral("message")).toString();

  QString value;
  if (!progress.isEmpty() && !total.isEmpty()) {
    value = QStringLiteral("%1/%2").arg(progress, total);
  } else if (!progress.isEmpty()) {
    value = progress;
  }

  QStringList parts;
  if (!marker.isEmpty()) {
    parts.push_back(QStringLiteral("id=%1").arg(marker));
  }
  if (!value.isEmpty()) {
    parts.push_back(QStringLiteral("progress=%1").arg(value));
  }
  if (!message.isEmpty()) {
    parts.push_back(message);
  }

  if (!parts.isEmpty()) {
    return parts.join(QStringLiteral(" | "));
  }

  return QStringLiteral("%1 notification: %2").arg(method, compact_json(params));
}

} // namespace

McpNotificationKind notification_kind(const QString& method)
{
  if (method == QString::fromUtf8(kMcpLoggingMessageNotificationMethod)
      || method == QString::fromUtf8(kMcpLoggingNotificationMethod)) {
    return McpNotificationKind::LoggingMessage;
  }
  if (method == QString::fromUtf8(kMcpProgressNotificationMethod)) {
    return McpNotificationKind::Progress;
  }
  return McpNotificationKind::Other;
}

QString notification_kind_name(McpNotificationKind kind)
{
  switch (kind) {
  case McpNotificationKind::LoggingMessage:
    return QStringLiteral("logging");
  case McpNotificationKind::Progress:
    return QStringLiteral("progress");
  case McpNotificationKind::Other:
    return QStringLiteral("other");
  }

  return QStringLiteral("other");
}

QJsonObject notification_params_object(const JsonRpcNotification& notification)
{
  if (!notification.params.has_value() || !notification.params->isObject()) {
    return {};
  }

  return notification.params->toObject();
}

QString notification_summary(const JsonRpcNotification& notification)
{
  const auto params = notification_params_object(notification);
  switch (notification_kind(notification.method)) {
  case McpNotificationKind::LoggingMessage:
    return logging_summary(notification.method, params);
  case McpNotificationKind::Progress:
    return progress_summary(notification.method, params);
  case McpNotificationKind::Other:
    break;
  }

  if (!params.isEmpty()) {
    return QStringLiteral("%1 notification: %2").arg(notification.method, compact_json(params));
  }
  return QStringLiteral("%1 notification").arg(notification.method);
}

} // namespace mcp
