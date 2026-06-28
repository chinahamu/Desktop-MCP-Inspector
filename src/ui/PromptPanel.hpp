#pragma once

#include "McpPrompt.hpp"

#include <QJsonObject>
#include <QString>
#include <QWidget>

#include <optional>
#include <vector>

class QLabel;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QVBoxLayout;

namespace ui {

class PromptPanel final : public QWidget
{
  Q_OBJECT

public:
  explicit PromptPanel(QWidget* parent = nullptr);

  void set_prompts(std::vector<mcp::McpPrompt> prompts);
  void clear_prompts();
  void set_get_result(const QJsonObject& result);
  void set_get_error(const QString& message);

  [[nodiscard]] std::optional<mcp::McpPrompt> selected_prompt() const;
  [[nodiscard]] std::optional<QJsonObject> current_arguments() const;

signals:
  void promptGetRequested(const QString& prompt_name, const QJsonObject& arguments);

private:
  void setup_prompt_list(QVBoxLayout* root_layout);
  void setup_prompt_detail(QVBoxLayout* root_layout);
  void connect_controls();
  void update_detail_for_current_prompt();
  void validate_arguments();
  void get_selected_prompt();
  [[nodiscard]] std::optional<int> selected_prompt_index() const;
  [[nodiscard]] std::optional<QJsonObject> parse_current_arguments(QString* error_message) const;
  void set_preview_json(const QJsonObject& object);

  std::vector<mcp::McpPrompt> prompts_;
  QListWidget* prompts_list_ = nullptr;
  QPlainTextEdit* description_view_ = nullptr;
  QPlainTextEdit* arguments_schema_view_ = nullptr;
  QPlainTextEdit* arguments_editor_ = nullptr;
  QLabel* validation_status_ = nullptr;
  QPushButton* validate_button_ = nullptr;
  QPushButton* get_button_ = nullptr;
  QPlainTextEdit* preview_view_ = nullptr;
};

} // namespace ui
