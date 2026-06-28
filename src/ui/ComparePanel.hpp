#pragma once

#include "../compare/ServerDiff.hpp"

#include <QWidget>

#include <optional>

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;

namespace ui {

class ComparePanel final : public QWidget
{
  Q_OBJECT

public:
  explicit ComparePanel(QWidget* parent = nullptr);

  void set_diff_result(compare::ServerDiffResult result);
  void clear_diff();

  [[nodiscard]] const compare::ServerDiffResult& diff_result() const;
  [[nodiscard]] std::optional<compare::DiffEntry> selected_entry() const;

signals:
  void compareRequested();
  void exportReportRequested();

private:
  void setup_ui();
  void connect_controls();
  void update_summary();
  void update_entries_table();
  void update_detail_for_current_row();
  [[nodiscard]] QString detail_text(const compare::DiffEntry& entry) const;

  compare::ServerDiffResult result_;
  QLabel* summary_label_ = nullptr;
  QLabel* source_label_ = nullptr;
  QPushButton* compare_button_ = nullptr;
  QPushButton* export_button_ = nullptr;
  QTableWidget* entries_table_ = nullptr;
  QPlainTextEdit* detail_view_ = nullptr;
};

} // namespace ui
