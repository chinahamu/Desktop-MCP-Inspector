#include "HTMLReportWriter.hpp"

#include "RiskFinding.hpp"

#include <QTextStream>

namespace report {
namespace {

[[nodiscard]] QString html(QString value)
{
  return value.toHtmlEscaped();
}

[[nodiscard]] QString generated_at_to_string(const QDateTime& generated_at)
{
  if (!generated_at.isValid()) {
    return QStringLiteral("unknown");
  }

  return generated_at.toUTC().toString(Qt::ISODateWithMs);
}

[[nodiscard]] QString pass_status(bool passed)
{
  return passed ? QStringLiteral("pass") : QStringLiteral("review required");
}

void write_security_findings(QTextStream& stream, const security::SecurityScanResult& security)
{
  stream << "<h2>Security findings</h2>\n";
  if (security.findings.empty()) {
    stream << "<p>No security findings were detected.</p>\n";
    return;
  }

  stream << "<table>\n";
  stream << "<thead><tr><th>Severity</th><th>Category</th><th>Title</th><th>Subject</th><th>Detail</th><th>Recommendation</th></tr></thead>\n";
  stream << "<tbody>\n";
  for (const auto& finding : security.findings) {
    stream << "<tr>"
           << "<td>" << html(security::to_string(finding.severity)) << "</td>"
           << "<td>" << html(security::to_string(finding.category)) << "</td>"
           << "<td>" << html(finding.title) << "</td>"
           << "<td>" << html(finding.subject) << "</td>"
           << "<td>" << html(finding.detail) << "</td>"
           << "<td>" << html(finding.recommendation) << "</td>"
           << "</tr>\n";
  }
  stream << "</tbody>\n</table>\n";
}

void write_timeline_summary(QTextStream& stream, const TimelineSummary& timeline)
{
  stream << "<h2>Timeline summary</h2>\n";
  stream << "<ul>\n";
  stream << "<li>Total events: " << timeline.total_events << "</li>\n";
  stream << "<li>Directions: inbound " << timeline.inbound_count << ", outbound " << timeline.outbound_count
         << ", internal " << timeline.internal_count << "</li>\n";
  stream << "<li>Kinds: request " << timeline.request_count << ", response " << timeline.response_count
         << ", notification " << timeline.notification_count << ", stderr " << timeline.stderr_count
         << ", transport error " << timeline.transport_error_count << ", lifecycle " << timeline.lifecycle_count
         << "</li>\n";
  stream << "<li>Status counts: errors " << timeline.error_count << ", logs " << timeline.log_count << "</li>\n";
  stream << "<li>Average measured duration: "
         << html(format_average_duration(timeline.total_duration_ms, timeline.measured_duration_count)) << "</li>\n";
  stream << "</ul>\n";

  if (timeline.methods.empty()) {
    stream << "<p>No timeline methods were recorded.</p>\n";
    return;
  }

  stream << "<h3>Timeline method summary</h3>\n";
  stream << "<table>\n";
  stream << "<thead><tr><th>Method</th><th>Events</th><th>Errors</th><th>Average duration</th></tr></thead>\n";
  stream << "<tbody>\n";
  for (const auto& method : timeline.methods) {
    stream << "<tr>"
           << "<td>" << html(method.method) << "</td>"
           << "<td>" << method.count << "</td>"
           << "<td>" << method.error_count << "</td>"
           << "<td>" << html(format_average_duration(method.total_duration_ms, method.measured_duration_count)) << "</td>"
           << "</tr>\n";
  }
  stream << "</tbody>\n</table>\n";
}

} // namespace

QString HTMLReportWriter::write(const ReportModel& model) const
{
  QString output;
  QTextStream stream(&output);

  stream << "<!doctype html>\n<html lang=\"en\">\n<head>\n";
  stream << "<meta charset=\"utf-8\">\n";
  stream << "<title>" << html(model.title) << "</title>\n";
  stream << "<style>body{font-family:system-ui,sans-serif;line-height:1.5;margin:2rem;}table{border-collapse:collapse;width:100%;}th,td{border:1px solid #ccc;padding:0.4rem;vertical-align:top;}th{background:#f5f5f5;text-align:left;}</style>\n";
  stream << "</head>\n<body>\n";
  stream << "<h1>" << html(model.title) << "</h1>\n";
  stream << "<p><strong>Generated:</strong> " << html(generated_at_to_string(model.generated_at)) << "</p>\n";
  if (!model.profile_name.isEmpty()) {
    stream << "<p><strong>Profile:</strong> " << html(model.profile_name) << "</p>\n";
  }

  stream << "<h2>Security score</h2>\n";
  stream << "<ul>\n";
  stream << "<li>Score: " << model.security.score.value << "/100 ("
         << html(security::to_string(model.security.score.severity)) << ")</li>\n";
  stream << "<li>Summary: " << html(model.security.score.summary) << "</li>\n";
  stream << "<li>Rules evaluated: " << model.security.rule_count << "</li>\n";
  stream << "<li>Findings: " << static_cast<qsizetype>(model.security.findings.size()) << "</li>\n";
  stream << "<li>Status: " << html(pass_status(model.security.passed)) << "</li>\n";
  stream << "</ul>\n";

  write_security_findings(stream, model.security);
  write_timeline_summary(stream, model.timeline);

  stream << "</body>\n</html>\n";
  return output;
}

} // namespace report
