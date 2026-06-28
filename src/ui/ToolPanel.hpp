#pragma once

#include "McpTool.hpp"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QWidget>

#include <optional>
#include <vector>

class QFormLayout;
class QLabel;
class QLayout;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QRadioButton;
class QVBoxLayout;

namespace ui {

enum class SchemaValueType {
  String,
  Number,
  Integer,
  Boolean,
  Object,
  Array,
  Enum,
  Unsupported,
};

struct SchemaFormField
{
  QString name;
  QString path;
  QString label;
  QString description;
  SchemaValueType type = SchemaValueType::Unsupported;
  bool required = false;
  QJsonArray enum_values;
  QJsonObject raw_schema;
  std::vector<SchemaFormField> children;
};

struct SchemaFormModel
{
  bool valid = false;
  QString error_message;
  QJsonObject raw_schema;
  std::vector<SchemaFormField> fields;
};

[[nodiscard]] SchemaFormModel parse_json_schema_subset(const QJsonObject& schema);
[[nodiscard]] QString schema_value_type_name(SchemaValueType type);

class SchemaFormWidget final : public QWidget
{
public:
  explicit SchemaFormWidget(QWidget* parent = nullptr);

  void set_schema(const QJsonObject& schema);
  void clear_values();

  [[nodiscard]] bool is_available() const;
  [[nodiscard]] QString error_message() const;
  [[nodiscard]] std::optional<QJsonObject> arguments(QString* error_message = nullptr) const;
  [[nodiscard]] bool validate(QString* error_message = nullptr) const;

private:
  struct FieldBinding
  {
    SchemaFormField field;
    QWidget* editor = nullptr;
    std::vector<FieldBinding> children;
  };

  void clear_layout(QLayout* layout);
  void add_fields_to_layout(
      const std::vector<SchemaFormField>& fields,
      QFormLayout* layout,
      std::vector<FieldBinding>* bindings);
  [[nodiscard]] QWidget* make_field_editor(const SchemaFormField& field);
  [[nodiscard]] QJsonObject object_from_bindings(
      const std::vector<FieldBinding>& bindings,
      QString* error_message,
      bool* ok) const;
  [[nodiscard]] std::optional<QJsonValue> value_from_binding(
      const FieldBinding& binding,
      QString* error_message,
      bool* ok) const;
  [[nodiscard]] bool binding_has_value(const FieldBinding& binding) const;
  [[nodiscard]] QString field_display_name(const SchemaFormField& field) const;

  static QString editor_object_name(const QString& path);
  static QString json_value_label(const QJsonValue& value);

  SchemaFormModel model_;
  QVBoxLayout* root_layout_ = nullptr;
  QLabel* status_label_ = nullptr;
  QFormLayout* fields_layout_ = nullptr;
  std::vector<FieldBinding> bindings_;
};

class ToolPanel final : public QWidget
{
  Q_OBJECT

public:
  explicit ToolPanel(QWidget* parent = nullptr);

  void set_tools(std::vector<mcp::McpTool> tools);
  void clear_tools();
  void set_call_result(const QJsonObject& result);
  void set_call_error(const QString& message);

  [[nodiscard]] std::optional<mcp::McpTool> selected_tool() const;
  [[nodiscard]] std::optional<QJsonObject> current_arguments() const;

signals:
  void toolCallRequested(const QString& tool_name, const QJsonObject& arguments);

private:
  void setup_tool_list(QVBoxLayout* root_layout);
  void setup_tool_detail(QVBoxLayout* root_layout);
  void connect_controls();
  void update_detail_for_current_tool();
  void update_argument_mode_controls();
  void validate_arguments();
  void run_selected_tool();
  [[nodiscard]] bool is_form_mode_active() const;
  [[nodiscard]] std::optional<int> selected_tool_index() const;
  [[nodiscard]] std::optional<QJsonObject> parse_current_arguments(QString* error_message) const;
  void set_result_json(const QJsonObject& object);

  std::vector<mcp::McpTool> tools_;
  QListWidget* tools_list_ = nullptr;
  QPlainTextEdit* description_view_ = nullptr;
  QPlainTextEdit* schema_view_ = nullptr;
  QRadioButton* json_mode_button_ = nullptr;
  QRadioButton* form_mode_button_ = nullptr;
  QPlainTextEdit* arguments_editor_ = nullptr;
  SchemaFormWidget* schema_form_ = nullptr;
  QLabel* validation_status_ = nullptr;
  QPushButton* validate_button_ = nullptr;
  QPushButton* call_button_ = nullptr;
  QPlainTextEdit* result_view_ = nullptr;
};

} // namespace ui
