#include "ReportModel.hpp"

#include <algorithm>
#include <iterator>
#include <utility>

namespace report {
namespace {

void record_method(TimelineSummary& summary, const timeline::TimelineEvent& event)
{
  auto method = event.method.trimmed();
  if (method.isEmpty()) {
    method = QStringLiteral("(none)");
  }

  auto iterator = std::find_if(
      summary.methods.begin(),
      summary.methods.end(),
      [&method](const TimelineMethodSummary& item) { return item.method == method; });

  if (iterator == summary.methods.end()) {
    TimelineMethodSummary method_summary;
    method_summary.method = method;
    summary.methods.push_back(method_summary);
    iterator = std::prev(summary.methods.end());
  }

  ++iterator->count;
  if (event.status == timeline::TimelineStatus::Error) {
    ++iterator->error_count;
  }
  if (event.duration_ms.has_value()) {
    iterator->total_duration_ms += *event.duration_ms;
    ++iterator->measured_duration_count;
  }
}

} // namespace

QString default_report_title()
{
  return QStringLiteral("Desktop MCP Inspector Report");
}

QString format_average_duration(qint64 total_duration_ms, qsizetype measured_duration_count)
{
  if (measured_duration_count <= 0) {
    return QStringLiteral("n/a");
  }

  const auto average = static_cast<double>(total_duration_ms) / static_cast<double>(measured_duration_count);
  return QStringLiteral("%1 ms").arg(average, 0, 'f', 1);
}

TimelineSummary summarize_timeline_events(const std::vector<timeline::TimelineEvent>& events)
{
  TimelineSummary summary;
  summary.total_events = static_cast<qsizetype>(events.size());

  for (const auto& event : events) {
    switch (event.direction) {
    case timeline::TimelineDirection::Inbound:
      ++summary.inbound_count;
      break;
    case timeline::TimelineDirection::Outbound:
      ++summary.outbound_count;
      break;
    case timeline::TimelineDirection::Internal:
      ++summary.internal_count;
      break;
    }

    switch (event.kind) {
    case timeline::TimelineEventKind::Request:
      ++summary.request_count;
      break;
    case timeline::TimelineEventKind::Response:
      ++summary.response_count;
      break;
    case timeline::TimelineEventKind::Notification:
      ++summary.notification_count;
      break;
    case timeline::TimelineEventKind::Stderr:
      ++summary.stderr_count;
      break;
    case timeline::TimelineEventKind::TransportError:
      ++summary.transport_error_count;
      break;
    case timeline::TimelineEventKind::Lifecycle:
      ++summary.lifecycle_count;
      break;
    }

    if (event.status == timeline::TimelineStatus::Error) {
      ++summary.error_count;
    }
    if (event.status == timeline::TimelineStatus::Log) {
      ++summary.log_count;
    }
    if (event.duration_ms.has_value()) {
      summary.total_duration_ms += *event.duration_ms;
      ++summary.measured_duration_count;
    }

    record_method(summary, event);
  }

  std::sort(
      summary.methods.begin(),
      summary.methods.end(),
      [](const TimelineMethodSummary& lhs, const TimelineMethodSummary& rhs) {
        if (lhs.count != rhs.count) {
          return lhs.count > rhs.count;
        }
        return lhs.method < rhs.method;
      });

  return summary;
}

ReportModel make_report_model(
    security::SecurityScanResult security_result,
    const std::vector<timeline::TimelineEvent>& timeline_events,
    const QString& title,
    const QString& profile_name,
    const QDateTime& generated_at)
{
  ReportModel model;
  model.title = title.trimmed().isEmpty() ? default_report_title() : title.trimmed();
  model.generated_at = generated_at.isValid() ? generated_at.toUTC() : QDateTime::currentDateTimeUtc();
  model.profile_name = profile_name.trimmed();
  model.security = std::move(security_result);
  model.timeline = summarize_timeline_events(timeline_events);
  return model;
}

} // namespace report
