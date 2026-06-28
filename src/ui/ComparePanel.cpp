#include "ComparePanel.hpp"

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

ComparePanel::ComparePanel(QWidget* parent)
  : QWidget(parent)
{
  setup_ui();
  connect_controls();
  update_summary();
  update_entries_table();
}

void ComparePanel::set_diff_result(compare::ServerDiffResult result)
{
  result_ = std::move(result);
  update_summary();
  update_entries_table();
  update_detail_for_current_row();
}

void ComparePanel::clear_diff()
{
  result_ = compare::ServerDiffResult{};
  update_summary();
  update_entries_table();
  detail_view_->setPlainText(tr("Run compare to inspect server surface changes."));
}

const compare::ServerDiffResult& ComparePanel::diff_result() const
{
  return result_;
}

std::optional<compare::DiffEntry> ComparePanel::selected_entry() const
{
  const auto row = entries_table_->currentRow();
  if (row < 0 || row >= static_cast<int>(result_.entries.size())) {
    return std::nullopt;
  }

  return result_.entries.at(static_cast<std::size_t>(row));
}

void ComparePanel::setup_ui()
{
  auto* root_layout = new QVBoxLayout(this);
  root_layout->setContentsMargins(0, 0, 0, 0);
  root_layout->setSpacing(8);

  auto* header_layout = new QHBoxLayout();
  header_layout->setContentsMargins(0, 0, 0, 0);

  summary_label_ = new QLabel(this);
  summary_label_->setObjectName(QStringLiteral("compareSummary"));
  summary_label_->setWordWrap(true);
  source_label_ = new QLabel(this);
  source_label_->setObjectName(QStringLiteral("compareSources"));
  source_label_->setWordWrap(true);

  compare_button_ = new QPushButton(tr("Run compare"), this);
  compare_button_->setObjectName(QStringLiteral("compareRunButton"));
  export_button_ = new QPushButton(tr("Export diff report"), this);
  export_button_->setObjectName(QStringLiteral("compareExportButton"));

  header_layout->addWidget(summary_label_, 2);
  header_layout->addWidget(source_label_, 2);
  header_layout->addWidget(compare_button_);
  header_layout->addWidget(export_button_);
  root_layout->addLayout(header_layout);

  entries_table_ = new QTableWidget(this);
  entries_table_->setObjectName(QStringLiteral("compareEntriesTable"));
  entries_table_->setColumnCount(5);
  entries_table_->setHorizontalHeaderLabels({
      tr("Section"),
      tr("Change"),
      tr("Breaking"),
      tr("Item"),
      tr("Title"),
  });
  entries_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  entries_table_->setSelectionMode(QAbstractItemView::SingleSelection);
  entries_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  entries_table_->horizontalHeader()->setStretchLastSection(true);
  entries_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  entries_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  entries_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
  entries_table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
  root_layout->addWidget(entries_table_, 2);

  root_layout->addWidget(new QLabel(tr("Diff detail"), this));
  detail_view_ = new QPlainTextEdit(this);
  detail_view_->setObjectName(QStringLiteral("compareEntryDetail"));
  detail_view_->setReadOnly(true);
  detail_view_->setMaximumBlockCount(600);
  detail_view_->setPlainText(tr("Run compare to inspect server surface changes."));
  root_layout->addWidget(detail_view_, 1);
}

void ComparePanel::connect_controls()
{
  connect(compare_button_, &QPushButton::clicked, this, &ComparePanel::compareRequested);
  connect(export_button_, &QPushButton::clicked, this, &ComparePanel::exportReportRequested);
  connect(entries_table_, &QTableWidget::currentCellChanged, this, [this](int, int, int, int) {
    update_detail_for_current_row();
  });
}

void ComparePanel::update_summary()
{
  const auto& summary = result_.summary;
  summary_label_->setText(tr("Changes: %1 total — added %2, removed %3, modified %4\nBreaking: %5, potential: %6")
      .arg(static_cast<qlonglong>(summary.total()))
      .arg(static_cast<qlonglong>(summary.added))
      .arg(static_cast<qlonglong>(summary.removed))
      .arg(static_cast<qlonglong>(summary.modified))
      .arg(static_cast<qlonglong>(summary.breaking))
      .arg(static_cast<qlonglong>(summary.potential)));

  source_label_->setText(tr("Before: %1\nAfter: %2")
      .arg(result_.before_name.isEmpty() ? tr("not selected") : result_.before_name,
           result_.after_name.isEmpty() ? tr("not selected") : result_.after_name));
}

void ComparePanel::update_entries_table()
{
  entries_table_->setRowCount(0);
  const auto row_count = static_cast<int>(result_.entries.size());
  entries_table_->setRowCount(row_count);

  for (int row = 0; row < row_count; ++row) {
    const auto& entry = result_.entries.at(static_cast<std::size_t>(row));
    entries_table_->setItem(row, 0, make_read_only_item(compare::to_string(entry.section)));
    entries_table_->setItem(row, 1, make_read_only_item(compare::to_string(entry.change)));
    entries_table_->setItem(row, 2, make_read_only_item(compare::to_string(entry.breaking)));
    entries_table_->setItem(row, 3, make_read_only_item(entry.identifier));
    entries_table_->setItem(row, 4, make_read_only_item(entry.title));
  }

  if (row_count > 0) {
    entries_table_->selectRow(0);
  }
}

void ComparePanel::update_detail_for_current_row()
{
  const auto entry = selected_entry();
  if (!entry.has_value()) {
    detail_view_->setPlainText(result_.entries.empty()
        ? tr("No server surface differences were detected.")
        : tr("Select a diff entry to inspect details."));
    return;
  }

  detail_view_->setPlainText(detail_text(*entry));
}

QString ComparePanel::detail_text(const compare::DiffEntry& entry) const
{
  QStringList lines;
  lines << tr("Section: %1").arg(compare::to_string(entry.section));
  lines << tr("Change: %1").arg(compare::to_string(entry.change));
  lines << tr("Breaking level: %1").arg(compare::to_string(entry.breaking));
  lines << tr("Item: %1").arg(entry.identifier);
  lines << tr("Title: %1").arg(entry.title);
  lines << tr("Detail:\n%1").arg(entry.detail);
  lines << tr("Before:\n%1").arg(entry.before_value.trimmed().isEmpty() ? QStringLiteral("—") : entry.before_value);
  lines << tr("After:\n%1").arg(entry.after_value.trimmed().isEmpty() ? QStringLiteral("—") : entry.after_value);
  return lines.join(QStringLiteral("\n\n"));
}

} // namespace ui
