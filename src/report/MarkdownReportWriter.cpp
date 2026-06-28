#include "MarkdownReportWriter.hpp"

#include "RiskFinding.hpp"

#include <QTextStream>

namespace report {
namespace {

[[nodiscard]] QString generated_at_to_string(const QDateTime& generated_at)
{
  if (!generated_at.isValid()) {
    return QStringLiteral("unknown");
  }

  return generated_at.toUTC().toString(Qt::ISODateWithMs);
}

[[nodiscard]] QString table_cell(QString value)
{
  value = value.trimmed();
  if (value.isEmpty()) {
    value = QStringLiteral("—");
  }
  value.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
  value.replace(QLatin1Char('\r'), QLatin1Char('\n'));
  value.replace(QLatin1Char('|'), QStringLiteral("\\|"));
  value.replace(QLatin1Char('\n'), QStringLiteral("<br>"));
  return value;
}

[[nodiscard]] QString pass_status(bool passed)
{
  return passed ? QStringLiteral("pass") : QStringLiteral("review required");
}

void write_findings(QTextStream& stream, const security::SecurityScanResult& security)
{
  stream << "## Security findings\n\n";
  if (security.findings.empty()) {
    stream << "No security findings were detected.\n\n";
    return;
  }

  stream << "| Severity | Category | Title | Subject | Recommendation |\n";
  stream << "| --- | --- | --- | --- | --- |\n";
  for (const auto& finding : security.findings) {
    stream << "| " << table_cell(security::to_string(finding.severity))
           << " | " << table_cell(security::to_string(finding.category))
           << " | " << table_cell(finding.title)
           << " | " << table_cell(finding.subject)
           << " | " << table_cell(finding.recommendation) << " |\n";
  }
  stream << "\n";
}

void write_timeline(QTextStream& stream, const TimelineSummary& timeline)
{
  stream << "## Timeline summary\n\n";
  stream << "- Total events: " << timeline.total_events << "\n";
  stream << "- Directions: inbound " << timeline.inbound_count << ", outbound " << timeline.outbound_count
         << ", internal " << timeline.internal_count << "\n";
  stream << "- Kinds: request " << timeline.request_count << ", response " << timeline.response_count
         << ", notification " << timeline.notification_count << ", stderr " << timeline.stderr_count
         << ", transport error " << timeline.transport_error_count << ", lifecycle " << timeline.lifecycle_count
         << "\n";
  stream << "- Statuses: errors " << timeline.error_count << ", logs " << timeline.log_count << "\n";
  stream << "- Average measured duration: "
         << format_average_duration(timeline.total_duration_ms, timeline.measured_duration_count) << "\n\n";

  if (timeline.methods.empty()) {
    stream << "No timeline methods were recorded.\n\n";
    return;
  }

  stream << "### Timeline method summary\n\n";
  stream << "| Method | Events | Errors | Average duration |\n";
  stream << "| --- | ---: | ---: | --- |\n";
  for (const auto& method : timeline.methods) {
    stream << "| " << table_cell(method.method)
           << " | " << method.count
           << " | " << method.error_count
           << " | " << table_cell(format_average_duration(method.total_duration_ms, method.measured_duration_count))
           << " |\n";
  }
  stream << "\n";
}

} // namespace

QString MarkdownReportWriter::write(const ReportModel& model) const
{
  QString output;
  QTextStream stream(&output);

  stream << "# " << model.title << "\n\n";
  stream << "- Generated: " << generated_at_to_string(model.generated_at) << "\n";
  if (!model.profile_name.isEmpty()) {
    stream << "- Profile: " << model.profile_name << "\n";
  }
  stream << "\n";

  stream << "## Security score\n\n";
  stream << "- Score: " << model.security.score.value << "/100 ("
         << security::to_string(model.security.score.severity) << ")\n";
  stream << "- Summary: " << model.security.score.summary << "\n";
  stream << "- Rules evaluated: " << model.security.rule_count << "\n";
  stream << "- Findings: " << static_cast<qsizetype>(model.security.findings.size()) << "\n";
  stream << "- Status: " << pass_status(model.security.passed) << "\n\n";

  write_findings(stream, model.security);
  write_timeline(stream, model.timeline);

  return output;
}

} // namespace report
