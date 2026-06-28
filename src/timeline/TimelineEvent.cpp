#include "TimelineEvent.hpp"

#include <QUuid>

#include <variant>

namespace timeline {
namespace {

template <class... Ts>
struct Overloaded : Ts...
{
  using Ts::operator()...;
};

template <class... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

[[nodiscard]] QString optional_duration_to_display(std::optional<qint64> duration_ms)
{
  if (!duration_ms.has_value()) {
    return QString{};
  }

  return QStringLiteral("%1 ms").arg(*duration_ms);
}

} // namespace

QString make_timeline_event_id()
{
  return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString timeline_direction_name(TimelineDirection direction)
{
  switch (direction) {
  case TimelineDirection::Inbound:
    return QStringLiteral("inbound");
  case TimelineDirection::Outbound:
    return QStringLiteral("outbound");
  case TimelineDirection::Internal:
    return QStringLiteral("internal");
  }

  return QStringLiteral("unknown");
}

QString timeline_event_kind_name(TimelineEventKind kind)
{
  switch (kind) {
  case TimelineEventKind::Request:
    return QStringLiteral("request");
  case TimelineEventKind::Response:
    return QStringLiteral("response");
  case TimelineEventKind::Notification:
    return QStringLiteral("notification");
  case TimelineEventKind::Stderr:
    return QStringLiteral("stderr");
  case TimelineEventKind::TransportError:
    return QStringLiteral("transport_error");
  case TimelineEventKind::Lifecycle:
    return QStringLiteral("lifecycle");
  }

  return QStringLiteral("unknown");
}

QString timeline_status_name(TimelineStatus status)
{
  switch (status) {
  case TimelineStatus::Pending:
    return QStringLiteral("pending");
  case TimelineStatus::Success:
    return QStringLiteral("success");
  case TimelineStatus::Error:
    return QStringLiteral("error");
  case TimelineStatus::Notification:
    return QStringLiteral("notification");
  case TimelineStatus::Log:
    return QStringLiteral("log");
  case TimelineStatus::Cancelled:
    return QStringLiteral("cancelled");
  case TimelineStatus::Timeout:
    return QStringLiteral("timeout");
  }

  return QStringLiteral("unknown");
}

QString request_id_to_display(const mcp::RequestId& request_id)
{
  return std::visit(
      Overloaded{
          [](std::nullptr_t) { return QStringLiteral("null"); },
          [](qint64 value) { return QString::number(value); },
          [](const QString& value) { return value; },
      },
      request_id.value());
}

QJsonValue request_id_to_json_value(const mcp::RequestId& request_id)
{
  return request_id.to_json_value();
}

QJsonObject to_json_object(const TimelineEvent& event)
{
  QJsonObject object{
      {QStringLiteral("eventId"), event.event_id},
      {QStringLiteral("timestamp"), event.timestamp.toUTC().toString(Qt::ISODateWithMs)},
      {QStringLiteral("direction"), timeline_direction_name(event.direction)},
      {QStringLiteral("kind"), timeline_event_kind_name(event.kind)},
      {QStringLiteral("status"), timeline_status_name(event.status)},
      {QStringLiteral("method"), event.method},
      {QStringLiteral("summary"), event.summary},
      {QStringLiteral("duration"), optional_duration_to_display(event.duration_ms)},
      {QStringLiteral("payload"), event.payload},
  };

  if (event.request_id.has_value()) {
    object.insert(QStringLiteral("requestId"), request_id_to_json_value(*event.request_id));
  }

  if (event.duration_ms.has_value()) {
    object.insert(QStringLiteral("durationMs"), *event.duration_ms);
  }

  return object;
}

} // namespace timeline
