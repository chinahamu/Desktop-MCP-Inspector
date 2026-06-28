#pragma once

#include "RiskFinding.hpp"
#include "SecurityScanner.hpp"

#include <QWidget>

#include <optional>
#include <vector>

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;

namespace ui {

class SecurityPanel final : public QWidget
{
  Q_OBJECT

public:
  explicit SecurityPanel(QWidget* parent = nullptr);

  void set_scan_result(security::SecurityScanResult result);
  void clear_findings();

  [[nodiscard]] const security::SecurityScanResult& scan_result() const;
  [[nodiscard]] std::optional<security::RiskFinding> selected_finding() const;

signals:
  void scanRequested();
  void exportReportRequested();

private:
  void setup_ui();
  void connect_controls();
  void update_summary();
  void update_findings_table();
  void update_detail_for_current_row();
  [[nodiscard]] QString detail_text(const security::RiskFinding& finding) const;

  security::SecurityScanResult result_;
  QLabel* score_label_ = nullptr;
  QLabel* rules_label_ = nullptr;
  QPushButton* scan_button_ = nullptr;
  QPushButton* export_button_ = nullptr;
  QTableWidget* findings_table_ = nullptr;
  QPlainTextEdit* detail_view_ = nullptr;
};

} // namespace ui
