#include "DiffMarkdownWriter.hpp"

#include <QDateTime>
#include <QTextStream>

namespace report {
namespace {

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

} // namespace

QString DiffMarkdownWriter::write(const compare::ServerDiffResult& diff) const
{
  QString output;
  QTextStream stream(&output);

  stream << "# Desktop MCP Inspector Diff Report\n\n";
  stream << "- Generated: " << QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs) << "\n";
  stream << "- Before: " << table_cell(diff.before_name) << "\n";
  stream << "- After: " << table_cell(diff.after_name) << "\n";
  stream << "- Changes: " << static_cast<qlonglong>(diff.summary.total())
         << " (added " << static_cast<qlonglong>(diff.summary.added)
         << ", removed " << static_cast<qlonglong>(diff.summary.removed)
         << ", modified " << static_cast<qlonglong>(diff.summary.modified) << ")\n";
  stream << "- Breaking: " << static_cast<qlonglong>(diff.summary.breaking)
         << ", potential: " << static_cast<qlonglong>(diff.summary.potential) << "\n\n";

  if (diff.entries.empty()) {
    stream << "No server surface differences were detected.\n";
    return output;
  }

  stream << "| Section | Change | Breaking | Item | Title | Detail | Before | After |\n";
  stream << "| --- | --- | --- | --- | --- | --- | --- | --- |\n";
  for (const auto& entry : diff.entries) {
    stream << "| " << table_cell(compare::to_string(entry.section))
           << " | " << table_cell(compare::to_string(entry.change))
           << " | " << table_cell(compare::to_string(entry.breaking))
           << " | " << table_cell(entry.identifier)
           << " | " << table_cell(entry.title)
           << " | " << table_cell(entry.detail)
           << " | " << table_cell(entry.before_value)
           << " | " << table_cell(entry.after_value)
           << " |\n";
  }
  stream << "\n";
  return output;
}

} // namespace report
