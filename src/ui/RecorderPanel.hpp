#pragma once

#include "RecorderReplay.hpp"

#include <QWidget>

#include <optional>

class QLabel;
class QListWidget;
class QPlainTextEdit;
class QPushButton;

namespace ui {

class RecorderPanel final : public QWidget
{
  Q_OBJECT

public:
  explicit RecorderPanel(QWidget* parent = nullptr);

  void set_test_cases(QVector<recorder::TestCase> test_cases);
  void set_replay_result(const recorder::ReplayRunResult& result);

  [[nodiscard]] std::optional<recorder::TestCase> selected_test_case() const;

signals:
  void replayRequested(const QString& test_case_id);
  void replayAllRequested();
  void importRequested();
  void exportRequested();

private:
  void refresh_list();
  void update_detail();
  [[nodiscard]] std::optional<int> selected_index() const;

  QVector<recorder::TestCase> test_cases_;
  QLabel* summary_label_ = nullptr;
  QListWidget* list_ = nullptr;
  QPlainTextEdit* detail_ = nullptr;
  QPlainTextEdit* replay_result_ = nullptr;
  QPushButton* replay_button_ = nullptr;
  QPushButton* replay_all_button_ = nullptr;
  QPushButton* import_button_ = nullptr;
  QPushButton* export_button_ = nullptr;
};

} // namespace ui
