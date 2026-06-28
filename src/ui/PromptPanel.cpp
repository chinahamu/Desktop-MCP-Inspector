#include "PromptPanel.hpp"

#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>

#include <utility>

namespace ui {
namespace {

[[nodiscard]] QString format_json(const QJsonObject& object)
{
  return QString::fromUtf8(QJsonDocument{object}.toJson(QJsonDocument::Indented));
}

[[nodiscard]] QJsonObject arguments_schema(const mcp::McpPrompt& prompt)
{
  QJsonArray arguments;
  for (const auto& argument : prompt.arguments) {
    arguments.append(mcp::to_json_object(argument));
  }

  return QJsonObject{
      {QStringLiteral("arguments"), arguments},
  };
}

[[nodiscard]] QJsonObject get_requested_result(const QString& prompt_name, const QJsonObject& arguments)
{
  return QJsonObject{
      {QStringLiteral("status"), QStringLiteral("get-requested")},
      {QStringLiteral("method"), QString::fromUtf8(mcp::kMcpPromptsGetMethod)},
      {QStringLiteral("params"), QJsonObject{
          {QStringLiteral("name"), prompt_name},
          {QStringLiteral("arguments"), arguments},
      }},
  };
}

} // namespace

PromptPanel::PromptPanel(QWidget* parent)
  : QWidget(parent)
{
  auto* root_layout = new QVBoxLayout(this);
  root_layout->setContentsMargins(0, 0, 0, 0);
  root_layout->setSpacing(8);

  auto* splitter = new QSplitter(Qt::Horizontal, this);
  auto* list_container = new QWidget(splitter);
  auto* list_layout = new QVBoxLayout(list_container);
  list_layout->setContentsMargins(0, 0, 0, 0);
  setup_prompt_list(list_layout);

  auto* detail_container = new QWidget(splitter);
  auto* detail_layout = new QVBoxLayout(detail_container);
  detail_layout->setContentsMargins(0, 0, 0, 0);
  setup_prompt_detail(detail_layout);

  splitter->addWidget(list_container);
  splitter->addWidget(detail_container);
  splitter->setStretchFactor(0, 1);
  splitter->setStretchFactor(1, 3);
  root_layout->addWidget(splitter, 1);

  connect_controls();
  update_detail_for_current_prompt();
}

void PromptPanel::set_prompts(std::vector<mcp::McpPrompt> prompts)
{
  prompts_ = std::move(prompts);
  prompts_list_->clear();

  for (std::size_t index = 0; index < prompts_.size(); ++index) {
    const auto& prompt = prompts_.at(index);
    auto* item = new QListWidgetItem(prompt.name, prompts_list_);
    item->setData(Qt::UserRole, static_cast<int>(index));
    item->setToolTip(prompt.description.value_or(prompt.name));
  }

  if (!prompts_.empty()) {
    prompts_list_->setCurrentRow(0);
  } else {
    update_detail_for_current_prompt();
  }
}

void PromptPanel::clear_prompts()
{
  prompts_.clear();
  prompts_list_->clear();
  update_detail_for_current_prompt();
}

void PromptPanel::set_get_result(const QJsonObject& result)
{
  set_preview_json(result);
}

void PromptPanel::set_get_error(const QString& message)
{
  set_preview_json(QJsonObject{
      {QStringLiteral("status"), QStringLiteral("error")},
      {QStringLiteral("message"), message},
  });
}

std::optional<mcp::McpPrompt> PromptPanel::selected_prompt() const
{
  const auto index = selected_prompt_index();
  if (!index.has_value()) {
    return std::nullopt;
  }

  return prompts_.at(static_cast<std::size_t>(*index));
}

std::optional<QJsonObject> PromptPanel::current_arguments() const
{
  QString unused_error_message;
  return parse_current_arguments(&unused_error_message);
}

void PromptPanel::setup_prompt_list(QVBoxLayout* root_layout)
{
  root_layout->addWidget(new QLabel(tr("Prompts"), this));

  prompts_list_ = new QListWidget(this);
  prompts_list_->setObjectName(QStringLiteral("promptsList"));
  prompts_list_->setSelectionMode(QAbstractItemView::SingleSelection);
  root_layout->addWidget(prompts_list_, 1);
}

void PromptPanel::setup_prompt_detail(QVBoxLayout* root_layout)
{
  root_layout->addWidget(new QLabel(tr("Description"), this));
  description_view_ = new QPlainTextEdit(this);
  description_view_->setObjectName(QStringLiteral("promptDescription"));
  description_view_->setReadOnly(true);
  description_view_->setMaximumBlockCount(200);
  root_layout->addWidget(description_view_);

  root_layout->addWidget(new QLabel(tr("Arguments"), this));
  arguments_schema_view_ = new QPlainTextEdit(this);
  arguments_schema_view_->setObjectName(QStringLiteral("promptArgumentsSchema"));
  arguments_schema_view_->setReadOnly(true);
  arguments_schema_view_->setMaximumBlockCount(300);
  root_layout->addWidget(arguments_schema_view_, 1);

  root_layout->addWidget(new QLabel(tr("Arguments JSON"), this));
  arguments_editor_ = new QPlainTextEdit(this);
  arguments_editor_->setObjectName(QStringLiteral("promptArguments"));
  arguments_editor_->setPlainText(QStringLiteral("{}"));
  arguments_editor_->setPlaceholderText(tr("Enter a JSON object for prompts/get arguments."));
  root_layout->addWidget(arguments_editor_);

  validation_status_ = new QLabel(tr("Arguments JSON has not been validated."), this);
  validation_status_->setObjectName(QStringLiteral("promptValidationStatus"));

  validate_button_ = new QPushButton(tr("Validate JSON"), this);
  validate_button_->setObjectName(QStringLiteral("promptValidateButton"));
  get_button_ = new QPushButton(tr("Get prompt"), this);
  get_button_->setObjectName(QStringLiteral("promptGetButton"));

  auto* actions = new QHBoxLayout();
  actions->setContentsMargins(0, 0, 0, 0);
  actions->addWidget(validation_status_, 1);
  actions->addWidget(validate_button_);
  actions->addWidget(get_button_);
  root_layout->addLayout(actions);

  root_layout->addWidget(new QLabel(tr("Messages preview"), this));
  preview_view_ = new QPlainTextEdit(this);
  preview_view_->setObjectName(QStringLiteral("promptPreview"));
  preview_view_->setReadOnly(true);
  preview_view_->setPlaceholderText(tr("prompts/get results and errors will appear here."));
  root_layout->addWidget(preview_view_, 1);
}

void PromptPanel::connect_controls()
{
  connect(prompts_list_, &QListWidget::currentRowChanged, this, [this](int) {
    update_detail_for_current_prompt();
  });
  connect(validate_button_, &QPushButton::clicked, this, &PromptPanel::validate_arguments);
  connect(get_button_, &QPushButton::clicked, this, &PromptPanel::get_selected_prompt);
}

void PromptPanel::update_detail_for_current_prompt()
{
  const auto prompt = selected_prompt();
  get_button_->setEnabled(prompt.has_value());

  if (!prompt.has_value()) {
    description_view_->setPlainText(tr("Select a prompt to inspect its description and arguments."));
    arguments_schema_view_->clear();
    arguments_editor_->setPlainText(QStringLiteral("{}"));
    validation_status_->setText(tr("No prompt selected."));
    preview_view_->clear();
    return;
  }

  description_view_->setPlainText(prompt->description.value_or(tr("No description provided.")));
  arguments_schema_view_->setPlainText(format_json(arguments_schema(*prompt)));
  arguments_editor_->setPlainText(QStringLiteral("{}"));
  validation_status_->setText(tr("Arguments JSON has not been validated."));
}

void PromptPanel::validate_arguments()
{
  QString error_message;
  const auto arguments = parse_current_arguments(&error_message);
  if (arguments.has_value()) {
    validation_status_->setText(tr("Arguments JSON is valid."));
  } else {
    validation_status_->setText(error_message);
  }
}

void PromptPanel::get_selected_prompt()
{
  const auto prompt = selected_prompt();
  if (!prompt.has_value()) {
    set_get_error(tr("Select a prompt before running prompts/get."));
    return;
  }

  QString error_message;
  const auto arguments = parse_current_arguments(&error_message);
  if (!arguments.has_value()) {
    validation_status_->setText(error_message);
    set_get_error(error_message);
    return;
  }

  validation_status_->setText(tr("Arguments JSON is valid."));
  set_get_result(get_requested_result(prompt->name, *arguments));
  emit promptGetRequested(prompt->name, *arguments);
}

std::optional<int> PromptPanel::selected_prompt_index() const
{
  const auto* item = prompts_list_->currentItem();
  if (item == nullptr) {
    return std::nullopt;
  }

  const auto value = item->data(Qt::UserRole);
  if (!value.isValid()) {
    return std::nullopt;
  }

  const auto index = value.toInt();
  if (index < 0 || index >= static_cast<int>(prompts_.size())) {
    return std::nullopt;
  }

  return index;
}

std::optional<QJsonObject> PromptPanel::parse_current_arguments(QString* error_message) const
{
  const auto payload = arguments_editor_->toPlainText().trimmed().toUtf8();
  if (payload.isEmpty()) {
    return QJsonObject{};
  }

  QJsonParseError parse_error;
  const auto document = QJsonDocument::fromJson(payload, &parse_error);
  if (parse_error.error != QJsonParseError::NoError) {
    if (error_message != nullptr) {
      *error_message = tr("Invalid JSON: %1").arg(parse_error.errorString());
    }
    return std::nullopt;
  }

  if (!document.isObject()) {
    if (error_message != nullptr) {
      *error_message = tr("Arguments must be a JSON object.");
    }
    return std::nullopt;
  }

  return document.object();
}

void PromptPanel::set_preview_json(const QJsonObject& object)
{
  preview_view_->setPlainText(format_json(object));
}

} // namespace ui
