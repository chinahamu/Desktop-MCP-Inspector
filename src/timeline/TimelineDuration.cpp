#include "TimelineDuration.hpp"

#include <algorithm>

namespace timeline {

TimelineEvent request_event_from_pending_request(
    const mcp::PendingRequest& pending_request,
    QDateTime timestamp)
{
  TimelineEvent event;
  event.event_id = make_timeline_event_id();
  event.timestamp = timestamp;
  event.direction = TimelineDirection::Outbound;
  event.kind = TimelineEventKind::Request;
  event.status = TimelineStatus::Pending;
  event.request_id = pending_request.id;
  event.method = pending_request.method;
  event.summary = QStringLiteral("%1 request sent").arg(pending_request.method);
  event.payload = QJsonObject{
      {QStringLiteral("id"), request_id_to_json_value(pending_request.id)},
      {QStringLiteral("method"), pending_request.method},
      {QStringLiteral("timeoutMs"), pending_request.timeout_ms.value_or(0)},
  };

  if (pending_request.params.has_value()) {
    event.payload.insert(QStringLiteral("params"), *pending_request.params);
  }

  return event;
}

TimelineEvent response_event_from_pending_request(
    const mcp::PendingRequest& pending_request,
    const mcp::JsonRpcResponse& response,
    QDateTime received_at)
{
  TimelineEvent event;
  event.event_id = make_timeline_event_id();
  event.timestamp = received_at;
  event.direction = TimelineDirection::Inbound;
  event.kind = TimelineEventKind::Response;
  event.status = response.is_success() ? TimelineStatus::Success : TimelineStatus::Error;
  event.request_id = pending_request.id;
  event.method = pending_request.method;
  event.duration_ms = duration_ms_between(pending_request.created_at, received_at);
  event.summary = response.is_success()
      ? QStringLiteral("%1 response received in %2 ms").arg(pending_request.method).arg(*event.duration_ms)
      : QStringLiteral("%1 error response received in %2 ms")
            .arg(pending_request.method)
            .arg(*event.duration_ms);
  event.payload = QJsonObject{
      {QStringLiteral("id"), request_id_to_json_value(response.id)},
      {QStringLiteral("durationMs"), *event.duration_ms},
      {QStringLiteral("success"), response.is_success()},
  };

  if (response.result.has_value()) {
    event.payload.insert(QStringLiteral("result"), *response.result);
  }

  if (response.error.has_value()) {
    event.payload.insert(
        QStringLiteral("error"),
        QJsonObject{
            {QStringLiteral("code"), response.error->code},
            {QStringLiteral("message"), response.error->message},
        });
    if (response.error->data.has_value()) {
      auto error_object = event.payload.value(QStringLiteral("error")).toObject();
      error_object.insert(QStringLiteral("data"), *response.error->data);
      event.payload.insert(QStringLiteral("error"), error_object);
    }
  }

  return event;
}

qint64 duration_ms_between(const QDateTime& started_at, const QDateTime& finished_at)
{
  return std::max<qint64>(0, started_at.msecsTo(finished_at));
}

} // namespace timeline
