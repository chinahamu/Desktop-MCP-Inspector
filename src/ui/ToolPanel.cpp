#include "ToolPanel.hpp"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSet>
#include <QSplitter>
#include <QVBoxLayout>

#include <optional>
#include <utility>

namespace ui {
namespace {

[[nodiscard]] QString field_label(const QString& name, const QJsonObject& schema)
{
  const auto title = schema.value(QStringLiteral("title")).toString().trimmed();
  return title.isEmpty() ? name : title;
}

[[nodiscard]] QString field_description(const QJsonObject& schema)
{
  return schema.value(QStringLiteral("description")).toString().trimmed();
}

[[nodiscard]] QSet<QString> required_names(const QJsonObject& schema)
{
  QSet<QString> result;
  const auto required = schema.value(QStringLiteral("required"));
  if (!required.isArray()) {
    return result;
  }

  for (const auto& item : required.toArray()) {
    if (item.isString()) {
      result.insert(item.toString());
    }
  }

  return result;
}

[[nodiscard]] QString schema_type_name(const QJsonObject& schema)
{
  const auto type = schema.value(QStringLiteral("type"));
  if (type.isString()) {
    return type.toString();
  }

  if (type.isArray()) {
    for (const auto& candidate : type.toArray()) {
      if (candidate.isString() && candidate.toString() != QStringLiteral("null")) {
        return candidate.toString();
      }
    }
  }

  if (schema.value(QStringLiteral("properties")).isObject()) {
    return QStringLiteral("object");
  }

  return {};
}

[[nodiscard]] std::optional<SchemaValueType> parse_value_type(const QJsonObject& schema)
{
  const auto enum_value = schema.value(QStringLiteral("enum"));
  if (enum_value.isArray() && !enum_value.toArray().isEmpty()) {
    return SchemaValueType::Enum;
  }

  const auto type_name = schema_type_name(schema);
  if (type_name == QStringLiteral("string")) {
    return SchemaValueType::String;
  }
  if (type_name == QStringLiteral("number")) {
    return SchemaValueType::Number;
  }
  if (type_name == QStringLiteral("integer")) {
    return SchemaValueType::Integer;
  }
  if (type_name == QStringLiteral("boolean")) {
    return SchemaValueType::Boolean;
  }
  if (type_name == QStringLiteral("object")) {
    return SchemaValueType::Object;
  }
  if (type_name == QStringLiteral("array")) {
    return SchemaValueType::Array;
  }

  return std::nullopt;
}

[[nodiscard]] QString child_path(const QString& parent_path, const QString& name)
{
  if (parent_path.isEmpty()) {
    return name;
  }

  return QStringLiteral("%1.%2").arg(parent_path, name);
}

[[nodiscard]] bool parse_properties(
    const QJsonObject& schema,
    const QString& parent_path,
    std::vector<SchemaFormField>* fields,
    QString* error_message);

[[nodiscard]] std::optional<SchemaFormField> parse_field(
    const QString& name,
    const QJsonObject& schema,
    const QString& parent_path,
    bool required,
    QString* error_message)
{
  const auto parsed_type = parse_value_type(schema);
  if (!parsed_type.has_value()) {
    if (error_message != nullptr) {
      *error_message = QStringLiteral("Unsupported schema for field '%1'.").arg(child_path(parent_path, name));
    }
    return std::nullopt;
  }

  SchemaFormField field;
  field.name = name;
  field.path = child_path(parent_path, name);
  field.label = field_label(name, schema);
  field.description = field_description(schema);
  field.type = *parsed_type;
  field.required = required;
  field.raw_schema = schema;

  if (field.type == SchemaValueType::Enum) {
    field.enum_values = schema.value(QStringLiteral("enum")).toArray();
  }

  if (field.type == SchemaValueType::Object) {
    if (!parse_properties(schema, field.path, &field.children, error_message)) {
      return std::nullopt;
    }
  }

  return field;
}

[[nodiscard]] bool parse_properties(
    const QJsonObject& schema,
    const QString& parent_path,
    std::vector<SchemaFormField>* fields,
    QString* error_message)
{
  const auto properties_value = schema.value(QStringLiteral("properties"));
  if (properties_value.isUndefined()) {
    return true;
  }

  if (!properties_value.isObject()) {
    if (error_message != nullptr) {
      *error_message = parent_path.isEmpty()
          ? QStringLiteral("Input schema 'properties' must be an object.")
          : QStringLiteral("Input schema properties for '%1' must be an object.").arg(parent_path);
    }
    return false;
  }

  const auto properties = properties_value.toObject();
  const auto required = required_names(schema);
  for (auto iterator = properties.begin(); iterator != properties.end(); ++iterator) {
    if (!iterator.value().isObject()) {
      if (error_message != nullptr) {
        *error_message = QStringLiteral("Schema for field '%1' must be an object.")
            .arg(child_path(parent_path, iterator.key()));
      }
      return false;
    }

    auto field = parse_field(
        iterator.key(),
        iterator.value().toObject(),
        parent_path,
        required.contains(iterator.key()),
        error_message);
    if (!field.has_value()) {
      return false;
    }
    fields->push_back(std::move(*field));
  }

  return true;
}

[[nodiscard]] bool is_blank(const QString& value)
{
  return value.trimmed().isEmpty();
}

[[nodiscard]] QString required_suffix(const SchemaFormField& field)
{
  return field.required ? QStringLiteral(" *") : QString{};
}

[[nodiscard]] QString format_json(const QJsonObject& object)
{
  return QString::fromUtf8(QJsonDocument{object}.toJson(QJsonDocument::Indented));
}

[[nodiscard]] QJsonObject call_requested_result(const QString& tool_name, const QJsonObject& arguments)
{
  return QJsonObject{
      {QStringLiteral("status"), QStringLiteral("call-requested")},
      {QStringLiteral("method"), QString::fromUtf8(mcp::kMcpToolsCallMethod)},
      {QStringLiteral("params"), QJsonObject{
          {QStringLiteral("name"), tool_name},
          {QStringLiteral("arguments"), arguments},
      }},
  };
}

} // namespace

SchemaFormModel parse_json_schema_subset(const QJsonObject& schema)
{
  SchemaFormModel model;
  model.raw_schema = schema;

  const auto root_type = schema_type_name(schema);
  if (!root_type.isEmpty() && root_type != QStringLiteral("object")) {
    model.error_message = QStringLiteral("Tool input schema root must be an object.");
    return model;
  }

  QString error_message;
  if (!parse_properties(schema, QString{}, &model.fields, &error_message)) {
    model.error_message = error_message;
    return model;
  }

  model.valid = true;
  return model;
}

QString schema_value_type_name(SchemaValueType type)
{
  switch (type) {
  case SchemaValueType::String:
    return QStringLiteral("string");
  case SchemaValueType::Number:
    return QStringLiteral("number");
  case SchemaValueType::Integer:
    return QStringLiteral("integer");
  case SchemaValueType::Boolean:
    return QStringLiteral("boolean");
  case SchemaValueType::Object:
    return QStringLiteral("object");
  case SchemaValueType::Array:
    return QStringLiteral("array");
  case SchemaValueType::Enum:
    return QStringLiteral("enum");
  case SchemaValueType::Unsupported:
    return QStringLiteral("unsupported");
  }

  return QStringLiteral("unsupported");
}

SchemaFormWidget::SchemaFormWidget(QWidget* parent)
  : QWidget(parent)
{
  root_layout_ = new QVBoxLayout(this);
  root_layout_->setContentsMargins(0, 0, 0, 0);
  root_layout_->setSpacing(6);

  status_label_ = new QLabel(tr("No input schema loaded."), this);
  status_label_->setObjectName(QStringLiteral("schemaFormStatus"));
  status_label_->setWordWrap(true);
  root_layout_->addWidget(status_label_);

  fields_layout_ = new QFormLayout();
  fields_layout_->setContentsMargins(0, 0, 0, 0);
  fields_layout_->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  root_layout_->addLayout(fields_layout_);
}

void SchemaFormWidget::set_schema(const QJsonObject& schema)
{
  clear_layout(fields_layout_);
  bindings_.clear();

  model_ = parse_json_schema_subset(schema);
  if (!model_.valid) {
    status_label_->setText(
        tr("Schema form is unavailable: %1 Falling back to JSON input.")
            .arg(model_.error_message));
    return;
  }

  if (model_.fields.empty()) {
    status_label_->setText(tr("Schema form is available. This tool has no arguments."));
    return;
  }

  status_label_->setText(tr("Schema form is available. Required fields are marked with *."));
  add_fields_to_layout(model_.fields, fields_layout_, &bindings_);
}

void SchemaFormWidget::clear_values()
{
  for (const auto& binding : bindings_) {
    if (auto* line_edit = qobject_cast<QLineEdit*>(binding.editor)) {
      line_edit->clear();
    } else if (auto* check_box = qobject_cast<QCheckBox*>(binding.editor)) {
      check_box->setChecked(false);
    } else if (auto* combo_box = qobject_cast<QComboBox*>(binding.editor)) {
      combo_box->setCurrentIndex(0);
    } else if (auto* text_edit = qobject_cast<QPlainTextEdit*>(binding.editor)) {
      text_edit->clear();
    }

    for (const auto& child : binding.children) {
      if (auto* line_edit = qobject_cast<QLineEdit*>(child.editor)) {
        line_edit->clear();
      } else if (auto* check_box = qobject_cast<QCheckBox*>(child.editor)) {
        check_box->setChecked(false);
      } else if (auto* combo_box = qobject_cast<QComboBox*>(child.editor)) {
        combo_box->setCurrentIndex(0);
      } else if (auto* text_edit = qobject_cast<QPlainTextEdit*>(child.editor)) {
        text_edit->clear();
      }
    }
  }
}

bool SchemaFormWidget::is_available() const
{
  return model_.valid;
}

QString SchemaFormWidget::error_message() const
{
  return model_.error_message;
}

std::optional<QJsonObject> SchemaFormWidget::arguments(QString* error_message) const
{
  if (!model_.valid) {
    if (error_message != nullptr) {
      *error_message = tr("Schema form is unavailable: %1").arg(model_.error_message);
    }
    return std::nullopt;
  }

  bool ok = true;
  const auto object = object_from_bindings(bindings_, error_message, &ok);
  if (!ok) {
    return std::nullopt;
  }

  return object;
}

bool SchemaFormWidget::validate(QString* error_message) const
{
  return arguments(error_message).has_value();
}

void SchemaFormWidget::clear_layout(QLayout* layout)
{
  while (auto* item = layout->takeAt(0)) {
    if (auto* child_layout = item->layout()) {
      clear_layout(child_layout);
    }
    if (auto* widget = item->widget()) {
      delete widget;
    }
    delete item;
  }
}

void SchemaFormWidget::add_fields_to_layout(
    const std::vector<SchemaFormField>& fields,
    QFormLayout* layout,
    std::vector<FieldBinding>* bindings)
{
  for (const auto& field : fields) {
    FieldBinding binding;
    binding.field = field;

    if (field.type == SchemaValueType::Object) {
      auto* group = new QGroupBox(field_display_name(field) + required_suffix(field));
      group->setObjectName(editor_object_name(field.path));
      auto* group_layout = new QFormLayout(group);
      group_layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
      add_fields_to_layout(field.children, group_layout, &binding.children);
      binding.editor = group;
      layout->addRow(group);
    } else {
      auto* editor = make_field_editor(field);
      binding.editor = editor;
      layout->addRow(field_display_name(field) + required_suffix(field), editor);
    }

    bindings->push_back(std::move(binding));
  }
}

QWidget* SchemaFormWidget::make_field_editor(const SchemaFormField& field)
{
  QWidget* editor = nullptr;

  switch (field.type) {
  case SchemaValueType::String: {
    auto* line_edit = new QLineEdit();
    line_edit->setPlaceholderText(field.description);
    editor = line_edit;
    break;
  }
  case SchemaValueType::Number:
  case SchemaValueType::Integer: {
    auto* line_edit = new QLineEdit();
    line_edit->setPlaceholderText(schema_value_type_name(field.type));
    editor = line_edit;
    break;
  }
  case SchemaValueType::Boolean: {
    auto* check_box = new QCheckBox(tr("true"));
    editor = check_box;
    break;
  }
  case SchemaValueType::Enum: {
    auto* combo_box = new QComboBox();
    if (!field.required) {
      combo_box->addItem(tr("<not set>"), -1);
    }
    for (qsizetype index = 0; index < field.enum_values.size(); ++index) {
      combo_box->addItem(
          json_value_label(field.enum_values.at(index)),
          static_cast<int>(index));
    }
    editor = combo_box;
    break;
  }
  case SchemaValueType::Array: {
    auto* text_edit = new QPlainTextEdit();
    text_edit->setPlaceholderText(tr("Enter a JSON array, for example: []"));
    text_edit->setMaximumBlockCount(200);
    text_edit->setMinimumHeight(72);
    editor = text_edit;
    break;
  }
  case SchemaValueType::Object:
  case SchemaValueType::Unsupported:
    editor = new QLabel(tr("Unsupported field"));
    break;
  }

  editor->setObjectName(editor_object_name(field.path));
  if (!field.description.isEmpty()) {
    editor->setToolTip(field.description);
  }
  return editor;
}

QJsonObject SchemaFormWidget::object_from_bindings(
    const std::vector<FieldBinding>& bindings,
    QString* error_message,
    bool* ok) const
{
  QJsonObject object;

  for (const auto& binding : bindings) {
    const auto value = value_from_binding(binding, error_message, ok);
    if (ok != nullptr && !*ok) {
      return {};
    }
    if (value.has_value()) {
      object.insert(binding.field.name, *value);
    }
  }

  return object;
}

std::optional<QJsonValue> SchemaFormWidget::value_from_binding(
    const FieldBinding& binding,
    QString* error_message,
    bool* ok) const
{
  const auto fail = [&](const QString& message) -> std::optional<QJsonValue> {
    if (error_message != nullptr) {
      *error_message = message;
    }
    if (ok != nullptr) {
      *ok = false;
    }
    return std::nullopt;
  };

  switch (binding.field.type) {
  case SchemaValueType::String: {
    const auto* line_edit = qobject_cast<QLineEdit*>(binding.editor);
    const auto text = line_edit == nullptr ? QString{} : line_edit->text();
    if (is_blank(text)) {
      if (binding.field.required) {
        return fail(tr("%1 is required.").arg(field_display_name(binding.field)));
      }
      return std::nullopt;
    }
    return QJsonValue{text};
  }

  case SchemaValueType::Number: {
    const auto* line_edit = qobject_cast<QLineEdit*>(binding.editor);
    const auto text = line_edit == nullptr ? QString{} : line_edit->text().trimmed();
    if (text.isEmpty()) {
      if (binding.field.required) {
        return fail(tr("%1 is required.").arg(field_display_name(binding.field)));
      }
      return std::nullopt;
    }

    bool converted = false;
    const auto value = text.toDouble(&converted);
    if (!converted) {
      return fail(tr("%1 must be a number.").arg(field_display_name(binding.field)));
    }
    return QJsonValue{value};
  }

  case SchemaValueType::Integer: {
    const auto* line_edit = qobject_cast<QLineEdit*>(binding.editor);
    const auto text = line_edit == nullptr ? QString{} : line_edit->text().trimmed();
    if (text.isEmpty()) {
      if (binding.field.required) {
        return fail(tr("%1 is required.").arg(field_display_name(binding.field)));
      }
      return std::nullopt;
    }

    bool converted = false;
    const auto value = text.toLongLong(&converted);
    if (!converted) {
      return fail(tr("%1 must be an integer.").arg(field_display_name(binding.field)));
    }
    return QJsonValue{static_cast<double>(value)};
  }

  case SchemaValueType::Boolean: {
    const auto* check_box = qobject_cast<QCheckBox*>(binding.editor);
    const auto checked = check_box != nullptr && check_box->isChecked();
    if (!checked && !binding.field.required) {
      return std::nullopt;
    }
    return QJsonValue{checked};
  }

  case SchemaValueType::Enum: {
    const auto* combo_box = qobject_cast<QComboBox*>(binding.editor);
    const auto enum_index = combo_box == nullptr ? -1 : combo_box->currentData().toInt();
    if (enum_index < 0) {
      if (binding.field.required) {
        return fail(tr("%1 is required.").arg(field_display_name(binding.field)));
      }
      return std::nullopt;
    }
    if (static_cast<qsizetype>(enum_index) >= binding.field.enum_values.size()) {
      return fail(tr("%1 has an invalid enum selection.").arg(field_display_name(binding.field)));
    }
    return binding.field.enum_values.at(static_cast<qsizetype>(enum_index));
  }

  case SchemaValueType::Object: {
    const auto has_nested_value = binding_has_value(binding);
    if (!binding.field.required && !has_nested_value) {
      return std::nullopt;
    }

    bool nested_ok = true;
    const auto object = object_from_bindings(binding.children, error_message, &nested_ok);
    if (!nested_ok) {
      if (ok != nullptr) {
        *ok = false;
      }
      return std::nullopt;
    }
    return object;
  }

  case SchemaValueType::Array: {
    const auto* text_edit = qobject_cast<QPlainTextEdit*>(binding.editor);
    const auto text = text_edit == nullptr ? QString{} : text_edit->toPlainText().trimmed();
    if (text.isEmpty()) {
      if (binding.field.required) {
        return QJsonArray{};
      }
      return std::nullopt;
    }

    QJsonParseError parse_error;
    const auto document = QJsonDocument::fromJson(text.toUtf8(), &parse_error);
    if (parse_error.error != QJsonParseError::NoError) {
      return fail(tr("%1 must be a valid JSON array: %2")
          .arg(field_display_name(binding.field), parse_error.errorString()));
    }
    if (!document.isArray()) {
      return fail(tr("%1 must be a JSON array.").arg(field_display_name(binding.field)));
    }
    return document.array();
  }

  case SchemaValueType::Unsupported:
    return fail(tr("%1 uses an unsupported schema type.").arg(field_display_name(binding.field)));
  }

  return fail(tr("%1 uses an unsupported schema type.").arg(field_display_name(binding.field)));
}

bool SchemaFormWidget::binding_has_value(const FieldBinding& binding) const
{
  switch (binding.field.type) {
  case SchemaValueType::String:
  case SchemaValueType::Number:
  case SchemaValueType::Integer: {
    const auto* line_edit = qobject_cast<QLineEdit*>(binding.editor);
    return line_edit != nullptr && !is_blank(line_edit->text());
  }
  case SchemaValueType::Boolean: {
    const auto* check_box = qobject_cast<QCheckBox*>(binding.editor);
    return check_box != nullptr && check_box->isChecked();
  }
  case SchemaValueType::Enum: {
    const auto* combo_box = qobject_cast<QComboBox*>(binding.editor);
    return combo_box != nullptr && combo_box->currentData().toInt() >= 0;
  }
  case SchemaValueType::Array: {
    const auto* text_edit = qobject_cast<QPlainTextEdit*>(binding.editor);
    return text_edit != nullptr && !text_edit->toPlainText().trimmed().isEmpty();
  }
  case SchemaValueType::Object:
    for (const auto& child : binding.children) {
      if (binding_has_value(child)) {
        return true;
      }
    }
    return false;
  case SchemaValueType::Unsupported:
    return false;
  }

  return false;
}

QString SchemaFormWidget::field_display_name(const SchemaFormField& field) const
{
  return field.label.isEmpty() ? field.name : field.label;
}

QString SchemaFormWidget::editor_object_name(const QString& path)
{
  auto name = path;
  name.replace(QStringLiteral("."), QStringLiteral("_"));
  return QStringLiteral("schemaField_%1").arg(name);
}

QString SchemaFormWidget::json_value_label(const QJsonValue& value)
{
  if (value.isString()) {
    return value.toString();
  }
  if (value.isDouble()) {
    return QString::number(value.toDouble());
  }
  if (value.isBool()) {
    return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
  }
  if (value.isNull()) {
    return QStringLiteral("null");
  }
  if (value.isArray()) {
    return QString::fromUtf8(QJsonDocument{value.toArray()}.toJson(QJsonDocument::Compact));
  }
  if (value.isObject()) {
    return QString::fromUtf8(QJsonDocument{value.toObject()}.toJson(QJsonDocument::Compact));
  }

  return QStringLiteral("<undefined>");
}

ToolPanel::ToolPanel(QWidget* parent)
  : QWidget(parent)
{
  auto* root_layout = new QVBoxLayout(this);
  root_layout->setContentsMargins(0, 0, 0, 0);
  root_layout->setSpacing(8);

  auto* splitter = new QSplitter(Qt::Horizontal, this);
  auto* list_container = new QWidget(splitter);
  auto* list_layout = new QVBoxLayout(list_container);
  list_layout->setContentsMargins(0, 0, 0, 0);
  setup_tool_list(list_layout);

  auto* detail_container = new QWidget(splitter);
  auto* detail_layout = new QVBoxLayout(detail_container);
  detail_layout->setContentsMargins(0, 0, 0, 0);
  setup_tool_detail(detail_layout);

  splitter->addWidget(list_container);
  splitter->addWidget(detail_container);
  splitter->setStretchFactor(0, 1);
  splitter->setStretchFactor(1, 3);
  root_layout->addWidget(splitter, 1);

  connect_controls();
  update_detail_for_current_tool();
}

void ToolPanel::set_tools(std::vector<mcp::McpTool> tools)
{
  tools_ = std::move(tools);
  tools_list_->clear();

  for (std::size_t index = 0; index < tools_.size(); ++index) {
    const auto& tool = tools_.at(index);
    auto* item = new QListWidgetItem(tool.name, tools_list_);
    item->setData(Qt::UserRole, static_cast<int>(index));
    item->setToolTip(tool.description.value_or(tool.name));
  }

  if (!tools_.empty()) {
    tools_list_->setCurrentRow(0);
  } else {
    update_detail_for_current_tool();
  }
}

void ToolPanel::clear_tools()
{
  tools_.clear();
  tools_list_->clear();
  update_detail_for_current_tool();
}

void ToolPanel::set_call_result(const QJsonObject& result)
{
  set_result_json(result);
}

void ToolPanel::set_call_error(const QString& message)
{
  set_result_json(QJsonObject{
      {QStringLiteral("status"), QStringLiteral("error")},
      {QStringLiteral("message"), message},
  });
}

std::optional<mcp::McpTool> ToolPanel::selected_tool() const
{
  const auto index = selected_tool_index();
  if (!index.has_value()) {
    return std::nullopt;
  }

  return tools_.at(static_cast<std::size_t>(*index));
}

std::optional<QJsonObject> ToolPanel::current_arguments() const
{
  QString unused_error_message;
  return parse_current_arguments(&unused_error_message);
}

void ToolPanel::setup_tool_list(QVBoxLayout* root_layout)
{
  root_layout->addWidget(new QLabel(tr("Tools"), this));

  tools_list_ = new QListWidget(this);
  tools_list_->setObjectName(QStringLiteral("toolsList"));
  tools_list_->setSelectionMode(QAbstractItemView::SingleSelection);
  root_layout->addWidget(tools_list_, 1);
}

void ToolPanel::setup_tool_detail(QVBoxLayout* root_layout)
{
  root_layout->addWidget(new QLabel(tr("Description"), this));
  description_view_ = new QPlainTextEdit(this);
  description_view_->setObjectName(QStringLiteral("toolDescription"));
  description_view_->setReadOnly(true);
  description_view_->setMaximumBlockCount(200);
  root_layout->addWidget(description_view_);

  root_layout->addWidget(new QLabel(tr("Input schema"), this));
  schema_view_ = new QPlainTextEdit(this);
  schema_view_->setObjectName(QStringLiteral("toolSchema"));
  schema_view_->setReadOnly(true);
  schema_view_->setMaximumBlockCount(400);
  root_layout->addWidget(schema_view_, 1);

  auto* argument_header = new QHBoxLayout();
  argument_header->setContentsMargins(0, 0, 0, 0);
  argument_header->addWidget(new QLabel(tr("Arguments"), this));
  argument_header->addStretch(1);

  json_mode_button_ = new QRadioButton(tr("JSON"), this);
  json_mode_button_->setObjectName(QStringLiteral("toolJsonMode"));
  form_mode_button_ = new QRadioButton(tr("Form"), this);
  form_mode_button_->setObjectName(QStringLiteral("toolFormMode"));
  json_mode_button_->setChecked(true);

  argument_header->addWidget(json_mode_button_);
  argument_header->addWidget(form_mode_button_);
  root_layout->addLayout(argument_header);

  arguments_editor_ = new QPlainTextEdit(this);
  arguments_editor_->setObjectName(QStringLiteral("toolArguments"));
  arguments_editor_->setPlainText(QStringLiteral("{}"));
  arguments_editor_->setPlaceholderText(tr("Enter a JSON object for tools/call arguments."));
  root_layout->addWidget(arguments_editor_);

  schema_form_ = new SchemaFormWidget(this);
  schema_form_->setObjectName(QStringLiteral("toolSchemaForm"));
  schema_form_->hide();
  root_layout->addWidget(schema_form_);

  validation_status_ = new QLabel(tr("Arguments JSON has not been validated."), this);
  validation_status_->setObjectName(QStringLiteral("toolValidationStatus"));

  validate_button_ = new QPushButton(tr("Validate arguments"), this);
  validate_button_->setObjectName(QStringLiteral("toolValidateButton"));
  call_button_ = new QPushButton(tr("Run tool"), this);
  call_button_->setObjectName(QStringLiteral("toolCallButton"));

  auto* actions = new QHBoxLayout();
  actions->setContentsMargins(0, 0, 0, 0);
  actions->addWidget(validation_status_, 1);
  actions->addWidget(validate_button_);
  actions->addWidget(call_button_);
  root_layout->addLayout(actions);

  root_layout->addWidget(new QLabel(tr("Result"), this));
  result_view_ = new QPlainTextEdit(this);
  result_view_->setObjectName(QStringLiteral("toolResult"));
  result_view_->setReadOnly(true);
  result_view_->setPlaceholderText(tr("tools/call results and errors will appear here."));
  root_layout->addWidget(result_view_, 1);
}

void ToolPanel::connect_controls()
{
  connect(tools_list_, &QListWidget::currentRowChanged, this, [this](int) {
    update_detail_for_current_tool();
  });
  connect(json_mode_button_, &QRadioButton::toggled, this, [this](bool checked) {
    if (checked) {
      form_mode_button_->setChecked(false);
      update_argument_mode_controls();
    }
  });
  connect(form_mode_button_, &QRadioButton::toggled, this, [this](bool checked) {
    if (checked) {
      json_mode_button_->setChecked(false);
      update_argument_mode_controls();
    }
  });
  connect(validate_button_, &QPushButton::clicked, this, &ToolPanel::validate_arguments);
  connect(call_button_, &QPushButton::clicked, this, &ToolPanel::run_selected_tool);
}

void ToolPanel::update_detail_for_current_tool()
{
  const auto tool = selected_tool();
  call_button_->setEnabled(tool.has_value());

  if (!tool.has_value()) {
    description_view_->setPlainText(tr("Select a tool to inspect its description and input schema."));
    schema_view_->clear();
    arguments_editor_->setPlainText(QStringLiteral("{}"));
    schema_form_->set_schema(QJsonObject{});
    json_mode_button_->setChecked(true);
    form_mode_button_->setEnabled(false);
    validation_status_->setText(tr("No tool selected."));
    update_argument_mode_controls();
    return;
  }

  description_view_->setPlainText(tool->description.value_or(tr("No description provided.")));
  schema_view_->setPlainText(format_json(tool->input_schema));
  arguments_editor_->setPlainText(QStringLiteral("{}"));
  schema_form_->set_schema(tool->input_schema);
  json_mode_button_->setChecked(true);
  form_mode_button_->setEnabled(schema_form_->is_available());
  validation_status_->setText(schema_form_->is_available()
      ? tr("Arguments have not been validated.")
      : tr("Form unavailable: %1").arg(schema_form_->error_message()));
  update_argument_mode_controls();
}

void ToolPanel::update_argument_mode_controls()
{
  const auto use_form = form_mode_button_->isChecked() && schema_form_->is_available();

  arguments_editor_->setVisible(!use_form);
  schema_form_->setVisible(use_form);

  if (use_form) {
    validation_status_->setText(tr("Form arguments have not been validated."));
    return;
  }

  if (!schema_form_->is_available()) {
    form_mode_button_->setEnabled(false);
    if (!json_mode_button_->isChecked()) {
      json_mode_button_->setChecked(true);
    }
    validation_status_->setText(
        tr("Form unavailable: %1 JSON mode is active.").arg(schema_form_->error_message()));
    return;
  }

  validation_status_->setText(tr("Arguments JSON has not been validated."));
}

void ToolPanel::validate_arguments()
{
  QString error_message;
  const auto arguments = parse_current_arguments(&error_message);
  if (arguments.has_value()) {
    validation_status_->setText(
        is_form_mode_active() ? tr("Form arguments are valid.") : tr("Arguments JSON is valid."));
  } else {
    validation_status_->setText(error_message);
  }
}

void ToolPanel::run_selected_tool()
{
  const auto tool = selected_tool();
  if (!tool.has_value()) {
    set_call_error(tr("Select a tool before running tools/call."));
    return;
  }

  QString error_message;
  const auto arguments = parse_current_arguments(&error_message);
  if (!arguments.has_value()) {
    validation_status_->setText(error_message);
    set_call_error(error_message);
    return;
  }

  validation_status_->setText(
      is_form_mode_active() ? tr("Form arguments are valid.") : tr("Arguments JSON is valid."));
  set_call_result(call_requested_result(tool->name, *arguments));
  emit toolCallRequested(tool->name, *arguments);
}

bool ToolPanel::is_form_mode_active() const
{
  return form_mode_button_->isChecked() && schema_form_->is_available();
}

std::optional<int> ToolPanel::selected_tool_index() const
{
  const auto* item = tools_list_->currentItem();
  if (item == nullptr) {
    return std::nullopt;
  }

  const auto value = item->data(Qt::UserRole);
  if (!value.isValid()) {
    return std::nullopt;
  }

  const auto index = value.toInt();
  if (index < 0 || index >= static_cast<int>(tools_.size())) {
    return std::nullopt;
  }

  return index;
}

std::optional<QJsonObject> ToolPanel::parse_current_arguments(QString* error_message) const
{
  if (is_form_mode_active()) {
    return schema_form_->arguments(error_message);
  }

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

void ToolPanel::set_result_json(const QJsonObject& object)
{
  result_view_->setPlainText(format_json(object));
}

} // namespace ui
