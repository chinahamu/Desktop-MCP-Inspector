#pragma once

#include "SecurityScanner.hpp"
#include "TimelineEvent.hpp"

#include <QDateTime>
#include <QString>
#include <QtTypes>

#include <vector>

namespace report {

struct TimelineMethodSummary
{
  QString method;
  qsizetype count = 0;
  qsizetype error_count = 0;
  qint64 total_duration_ms = 0;
  qsizetype measured_duration_count = 0;

  friend bool operator==(const TimelineMethodSummary& lhs, const TimelineMethodSummary& rhs) = default;
};

struct TimelineSummary
{
  qsizetype total_events = 0;
  qsizetype inbound_count = 0;
  qsizetype outbound_count = 0;
  qsizetype internal_count = 0;
  qsizetype request_count = 0;
  qsizetype response_count = 0;
  qsizetype notification_count = 0;
  qsizetype stderr_count = 0;
  qsizetype transport_error_count = 0;
  qsizetype lifecycle_count = 0;
  qsizetype error_count = 0;
  qsizetype log_count = 0;
  qint64 total_duration_ms = 0;
  qsizetype measured_duration_count = 0;
  std::vector<TimelineMethodSummary> methods;

  friend bool operator==(const TimelineSummary& lhs, const TimelineSummary& rhs) = default;
};

struct ReportModel
{
  QString title;
  QDateTime generated_at;
  QString profile_name;
  security::SecurityScanResult security;
  TimelineSummary timeline;

  friend bool operator==(const ReportModel& lhs, const ReportModel& rhs) = default;
};

[[nodiscard]] QString default_report_title();
[[nodiscard]] QString format_average_duration(qint64 total_duration_ms, qsizetype measured_duration_count);
[[nodiscard]] TimelineSummary summarize_timeline_events(const std::vector<timeline::TimelineEvent>& events);
[[nodiscard]] ReportModel make_report_model(
    security::SecurityScanResult security_result,
    const std::vector<timeline::TimelineEvent>& timeline_events,
    const QString& title = QString{},
    const QString& profile_name = QString{},
    const QDateTime& generated_at = QDateTime::currentDateTimeUtc());

} // namespace report
