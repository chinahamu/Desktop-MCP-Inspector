#include "SecurityPanel.hpp"

#include <QAbstractItemView>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <utility>

namespace ui {
namespace {

[[nodiscard]] QTableWidgetItem* make_read_only_item(const QString& text)
{
  auto* item = new QTableWidgetItem(text);
  item->setFlags(item->flags() & ~Qt::ItemIsEditable);
  return item;
}

} // namespace

SecurityPanel::SecurityPanel(QWidget* parent)
  : QWidget(parent)
{
  setup_ui();
  connect_controls();
  update_summary();
  update_findings_table();
}

void SecurityPanel::set_scan_result(security::SecurityScanResult result)
{
  result_ = std::move(result);
  update_summary();
  update_findings_table();
  update_detail_for_current_row();
}

void SecurityPanel::clear_findings()
{
  result_ = security::SecurityScanResult{};
  update_summary();
  update_findings_table();
  detail_view_->setPlainText(tr("Run a security scan to inspect findings and recommendations."));
}

const security::SecurityScanResult& SecurityPanel::scan_result() const
{
  return result_;
}

std::optional<security::RiskFinding> SecurityPanel::selected_finding() const
{
  const auto row = findings_table_->currentRow();
  if (row < 0 || row >= static_cast<int>(result_.findings.size())) {
    return std::nullopt;
  }

  return result_.findings.at(static_cast<std::size_t>(row));
}

void SecurityPanel::setup_ui()
{
  auto* root_layout = new QVBoxLayout(this);
  root_layout->setContentsMargins(0, 0, 0, 0);
  root_layout->setSpacing(8);

  auto* header_layout = new QHBoxLayout();
  header_layout->setContentsMargins(0, 0, 0, 0);

  score_label_ = new QLabel(this);
  score_label_->setObjectName(QStringLiteral("securityScore"));
  score_label_->setWordWrap(true);
  rules_label_ = new QLabel(this);
  rules_label_->setObjectName(QStringLiteral("securityRules"));
  rules_label_->setWordWrap(true);

  scan_button_ = new QPushButton(tr("Run security scan"), this);
  scan_button_->setObjectName(QStringLiteral("securityScanButton"));
  export_button_ = new QPushButton(tr("Export report"), this);
  export_button_->setObjectName(QStringLiteral("securityReportExportButton"));

  header_layout->addWidget(score_label_, 2);
  header_layout->addWidget(rules_label_, 1);
  header_layout->addWidget(scan_button_);
  header_layout->addWidget(export_button_);
  root_layout->addLayout(header_layout);

  findings_table_ = new QTableWidget(this);
  findings_table_->setObjectName(QStringLiteral("securityFindingsTable"));
  findings_table_->setColumnCount(4);
  findings_table_->setHorizontalHeaderLabels({
      tr("Severity"),
      tr("Category"),
      tr("Title"),
      tr("Subject"),
  });
  findings_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  findings_table_->setSelectionMode(QAbstractItemView::SingleSelection);
  findings_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  findings_table_->horizontalHeader()->setStretchLastSection(true);
  findings_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  findings_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  findings_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
  root_layout->addWidget(findings_table_, 2);

  root_layout->addWidget(new QLabel(tr("Finding detail / recommendation"), this));
  detail_view_ = new QPlainTextEdit(this);
  detail_view_->setObjectName(QStringLiteral("securityFindingDetail"));
  detail_view_->setReadOnly(true);
  detail_view_->setMaximumBlockCount(400);
  detail_view_->setPlainText(tr("Run a security scan to inspect findings and recommendations."));
  root_layout->addWidget(detail_view_, 1);
}

void SecurityPanel::connect_controls()
{
  connect(scan_button_, &QPushButton::clicked, this, &SecurityPanel::scanRequested);
  connect(export_button_, &QPushButton::clicked, this, &SecurityPanel::exportReportRequested);
  connect(findings_table_, &QTableWidget::currentCellChanged, this, [this](int, int, int, int) {
    update_detail_for_current_row();
  });
}

void SecurityPanel::update_summary()
{
  score_label_->setText(tr("Security Score: %1/100 (%2) — %3")
      .arg(result_.score.value)
      .arg(security::to_string(result_.score.severity), result_.score.summary));
  rules_label_->setText(tr("Rules: %1\nFindings: %2\nStatus: %3")
      .arg(static_cast<qulonglong>(result_.rule_count))
      .arg(static_cast<qulonglong>(result_.findings.size()))
      .arg(result_.passed ? tr("pass") : tr("review required")));
}

void SecurityPanel::update_findings_table()
{
  findings_table_->setRowCount(0);
  const auto row_count = static_cast<int>(result_.findings.size());
  findings_table_->setRowCount(row_count);

  for (int row = 0; row < row_count; ++row) {
    const auto& finding = result_.findings.at(static_cast<std::size_t>(row));
    findings_table_->setItem(row, 0, make_read_only_item(security::to_string(finding.severity)));
    findings_table_->setItem(row, 1, make_read_only_item(security::to_string(finding.category)));
    findings_table_->setItem(row, 2, make_read_only_item(finding.title));
    findings_table_->setItem(row, 3, make_read_only_item(finding.subject));
  }

  if (row_count > 0) {
    findings_table_->selectRow(0);
  }
}

void SecurityPanel::update_detail_for_current_row()
{
  const auto finding = selected_finding();
  if (!finding.has_value()) {
    detail_view_->setPlainText(result_.findings.empty()
        ? tr("No security findings. The active profile and advertised MCP surfaces look low risk.")
        : tr("Select a finding to inspect the detail and recommendation."));
    return;
  }

  detail_view_->setPlainText(detail_text(*finding));
}

QString SecurityPanel::detail_text(const security::RiskFinding& finding) const
{
  QStringList lines;
  lines << tr("Title: %1").arg(finding.title);
  lines << tr("Severity: %1").arg(security::to_string(finding.severity));
  lines << tr("Category: %1").arg(security::to_string(finding.category));
  lines << tr("Subject: %1").arg(finding.subject);
  lines << tr("Detail:\n%1").arg(finding.detail);
  lines << tr("Recommendation:\n%1").arg(finding.recommendation);
  return lines.join(QStringLiteral("\n\n"));
}

} // namespace ui
