#include "RecorderPanel.hpp"

#include <QJsonDocument>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include <utility>

namespace ui {

RecorderPanel::RecorderPanel(QWidget* parent) : QWidget(parent)
{
  auto* layout = new QVBoxLayout(this);
  summary_label_ = new QLabel(this);
  list_ = new QListWidget(this);
  detail_ = new QPlainTextEdit(this);
  replay_result_ = new QPlainTextEdit(this);
  replay_button_ = new QPushButton(tr("Replay selected"), this);
  replay_all_button_ = new QPushButton(tr("Replay all"), this);
  import_button_ = new QPushButton(tr("Import JSON"), this);
  export_button_ = new QPushButton(tr("Export JSON"), this);
  detail_->setReadOnly(true);
  replay_result_->setReadOnly(true);
  layout->addWidget(summary_label_);
  layout->addWidget(list_, 1);
  layout->addWidget(detail_, 1);
  layout->addWidget(replay_result_, 1);
  layout->addWidget(replay_button_);
  layout->addWidget(replay_all_button_);
  layout->addWidget(import_button_);
  layout->addWidget(export_button_);
  connect(list_, &QListWidget::currentRowChanged, this, [this](int) { update_detail(); });
  connect(replay_button_, &QPushButton::clicked, this, [this]() {
    const auto selected = selected_test_case();
    if (selected.has_value()) emit replayRequested(selected->id);
  });
  connect(replay_all_button_, &QPushButton::clicked, this, &RecorderPanel::replayAllRequested);
  connect(import_button_, &QPushButton::clicked, this, &RecorderPanel::importRequested);
  connect(export_button_, &QPushButton::clicked, this, &RecorderPanel::exportRequested);
  refresh_list();
}

void RecorderPanel::set_test_cases(QVector<recorder::TestCase> test_cases)
{
  test_cases_ = std::move(test_cases);
  refresh_list();
}

void RecorderPanel::set_replay_result(const recorder::ReplayRunResult& result)
{
  replay_result_->setPlainText(result.summary());
}

std::optional<recorder::TestCase> RecorderPanel::selected_test_case() const
{
  const auto index = selected_index();
  if (!index.has_value()) return std::nullopt;
  return test_cases_.at(*index);
}

void RecorderPanel::refresh_list()
{
  list_->clear();
  for (int index = 0; index < test_cases_.size(); ++index) {
    list_->addItem(test_cases_.at(index).name);
  }
  summary_label_->setText(tr("%1 test case(s)").arg(test_cases_.size()));
  replay_button_->setEnabled(!test_cases_.empty());
  replay_all_button_->setEnabled(!test_cases_.empty());
  export_button_->setEnabled(!test_cases_.empty());
  if (!test_cases_.empty()) list_->setCurrentRow(0);
  update_detail();
}

void RecorderPanel::update_detail()
{
  const auto selected = selected_test_case();
  detail_->setPlainText(selected.has_value()
      ? QString::fromUtf8(QJsonDocument{recorder::to_json_object(*selected)}.toJson(QJsonDocument::Indented))
      : tr("No test case selected."));
}

std::optional<int> RecorderPanel::selected_index() const
{
  const auto row = list_->currentRow();
  if (row < 0 || row >= test_cases_.size()) return std::nullopt;
  return row;
}

} // namespace ui
