#include <catch2/catch_test_macros.hpp>

#include "McpTool.hpp"
#include "ToolPanel.hpp"

#include <QApplication>
#include <QByteArray>
#include <QCheckBox>
#include <QComboBox>
#include <QJsonArray>
#include <QJsonObject>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QString>

#include <optional>
#include <stdexcept>
#include <variant>

TEST_CASE("McpTool serializes and parses MCP tool descriptors", "[mcp][tools]")
{
  const mcp::McpTool tool{
      QStringLiteral("echo"),
      QStringLiteral("Echo a text payload"),
      QJsonObject{
          {QStringLiteral("type"), QStringLiteral("object")},
          {QStringLiteral("properties"), QJsonObject{{QStringLiteral("text"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}}}},
      },
  };

  const auto object = mcp::to_json_object(tool);
  REQUIRE(object.value(QStringLiteral("name")).toString() == QStringLiteral("echo"));
  REQUIRE(object.value(QStringLiteral("description")).toString() == QStringLiteral("Echo a text payload"));
  REQUIRE(object.value(QStringLiteral("inputSchema")).toObject().value(QStringLiteral("type")).toString() == QStringLiteral("object"));

  const auto parsed = mcp::tool_from_json_object(object);
  REQUIRE(parsed.has_value());
  REQUIRE(*parsed == tool);
}

TEST_CASE("tools/list request is tracked and supports pagination cursor", "[mcp][tools]")
{
  mcp::PendingRequestStore pending_requests;

  const auto request = mcp::make_tools_list_request(pending_requests, QStringLiteral("cursor-1"));

  REQUIRE(request.id == mcp::RequestId::number(1));
  REQUIRE(request.method == QStringLiteral("tools/list"));
  REQUIRE(request.params.has_value());
  REQUIRE(request.params->toObject().value(QStringLiteral("cursor")).toString() == QStringLiteral("cursor-1"));
  REQUIRE(pending_requests.contains(request.id));
}

TEST_CASE("tools/list response parses tools and next cursor", "[mcp][tools]")
{
  mcp::JsonRpcResponse response;
  response.id = mcp::RequestId::number(1);
  response.result = QJsonValue{QJsonObject{
      {QStringLiteral("tools"), QJsonArray{
          QJsonObject{
              {QStringLiteral("name"), QStringLiteral("echo")},
              {QStringLiteral("description"), QStringLiteral("Echo text")},
              {QStringLiteral("inputSchema"), QJsonObject{{QStringLiteral("type"), QStringLiteral("object")}}},
          },
      }},
      {QStringLiteral("nextCursor"), QStringLiteral("cursor-2")},
  }};

  const auto parsed = mcp::parse_tools_list_response(response);
  const auto* result = std::get_if<mcp::McpToolsListResult>(&parsed);

  REQUIRE(result != nullptr);
  REQUIRE(result->tools.size() == 1);
  REQUIRE(result->tools.front().name == QStringLiteral("echo"));
  REQUIRE(result->tools.front().description == QStringLiteral("Echo text"));
  REQUIRE(result->tools.front().input_schema.value(QStringLiteral("type")).toString() == QStringLiteral("object"));
  REQUIRE(result->next_cursor == QStringLiteral("cursor-2"));
}

TEST_CASE("tools/list validates malformed tool entries", "[mcp][tools]")
{
  const auto parsed = mcp::parse_tools_list_result(QJsonObject{
      {QStringLiteral("tools"), QJsonArray{
          QJsonObject{
              {QStringLiteral("name"), QStringLiteral("broken")},
              {QStringLiteral("inputSchema"), QStringLiteral("not-an-object")},
          },
      }},
  });

  const auto* error = std::get_if<mcp::McpToolParseError>(&parsed);
  REQUIRE(error != nullptr);
  REQUIRE(error->code == mcp::McpToolParseErrorCode::InvalidTool);
}

TEST_CASE("tools/call request serializes name and JSON object arguments", "[mcp][tools]")
{
  mcp::PendingRequestStore pending_requests;

  const auto request = mcp::make_tools_call_request(
      pending_requests,
      QStringLiteral("echo"),
      QJsonObject{{QStringLiteral("text"), QStringLiteral("hello")}});

  REQUIRE(request.id == mcp::RequestId::number(1));
  REQUIRE(request.method == QStringLiteral("tools/call"));
  REQUIRE(request.params.has_value());

  const auto params = request.params->toObject();
  REQUIRE(params.value(QStringLiteral("name")).toString() == QStringLiteral("echo"));
  REQUIRE(params.value(QStringLiteral("arguments")).toObject().value(QStringLiteral("text")).toString() == QStringLiteral("hello"));
  REQUIRE(pending_requests.contains(request.id));
}

TEST_CASE("tools/call rejects blank tool names", "[mcp][tools]")
{
  mcp::PendingRequestStore pending_requests;

  REQUIRE_THROWS_AS(
      mcp::make_tools_call_request(pending_requests, QStringLiteral("   "), QJsonObject{}),
      std::invalid_argument);
}

TEST_CASE("tools/call response parses content and isError flag", "[mcp][tools]")
{
  mcp::JsonRpcResponse response;
  response.id = mcp::RequestId::number(1);
  response.result = QJsonValue{QJsonObject{
      {QStringLiteral("content"), QJsonArray{
          QJsonObject{
              {QStringLiteral("type"), QStringLiteral("text")},
              {QStringLiteral("text"), QStringLiteral("hello")},
          },
      }},
      {QStringLiteral("isError"), false},
      {QStringLiteral("structuredContent"), QJsonObject{{QStringLiteral("text"), QStringLiteral("hello")}}},
  }};

  const auto parsed = mcp::parse_tools_call_response(response);
  const auto* result = std::get_if<mcp::McpToolCallResult>(&parsed);

  REQUIRE(result != nullptr);
  REQUIRE(result->content.size() == 1);
  REQUIRE(!result->is_error);
  REQUIRE(result->structured_content.has_value());
  REQUIRE(result->structured_content->value(QStringLiteral("text")).toString() == QStringLiteral("hello"));
  REQUIRE(result->raw_result.contains(QStringLiteral("content")));
}

TEST_CASE("tools response errors map to MCP tool parse errors", "[mcp][tools]")
{
  mcp::JsonRpcResponse response;
  response.id = mcp::RequestId::number(1);
  response.error = mcp::JsonRpcError{-32602, QStringLiteral("Invalid tool arguments"), std::nullopt};

  const auto parsed = mcp::parse_tools_call_response(response);
  const auto* error = std::get_if<mcp::McpToolParseError>(&parsed);

  REQUIRE(error != nullptr);
  REQUIRE(error->code == mcp::McpToolParseErrorCode::ResponseError);
  REQUIRE(error->message == QStringLiteral("Invalid tool arguments"));
}

namespace {

void ensure_qapplication()
{
  if (QApplication::instance() != nullptr) {
    return;
  }

  qputenv("QT_QPA_PLATFORM", QByteArray{"offscreen"});
  static int argc = 1;
  static char app_name[] = "desktop_mcp_inspector_schema_form_test";
  static char* argv[] = {app_name, nullptr};
  static QApplication app(argc, argv);
}

[[nodiscard]] QJsonObject schema_form_test_schema()
{
  return QJsonObject{
      {QStringLiteral("type"), QStringLiteral("object")},
      {QStringLiteral("required"), QJsonArray{QStringLiteral("text")}},
      {QStringLiteral("properties"), QJsonObject{
          {QStringLiteral("text"), QJsonObject{
              {QStringLiteral("type"), QStringLiteral("string")},
              {QStringLiteral("description"), QStringLiteral("Input text")},
          }},
          {QStringLiteral("retries"), QJsonObject{
              {QStringLiteral("type"), QStringLiteral("integer")},
          }},
          {QStringLiteral("dryRun"), QJsonObject{
              {QStringLiteral("type"), QStringLiteral("boolean")},
          }},
          {QStringLiteral("mode"), QJsonObject{
              {QStringLiteral("type"), QStringLiteral("string")},
              {QStringLiteral("enum"), QJsonArray{QStringLiteral("fast"), QStringLiteral("safe")}},
          }},
          {QStringLiteral("options"), QJsonObject{
              {QStringLiteral("type"), QStringLiteral("object")},
              {QStringLiteral("properties"), QJsonObject{
                  {QStringLiteral("tags"), QJsonObject{
                      {QStringLiteral("type"), QStringLiteral("array")},
                  }},
              }},
          }},
      }},
  };
}

} // namespace

TEST_CASE("ToolPanel constructs schema form controls without crashing", "[ui][schema]")
{
  ensure_qapplication();

  ui::ToolPanel panel;
  panel.set_tools(std::vector<mcp::McpTool>{mcp::McpTool{
      QStringLiteral("echo"),
      QStringLiteral("Echo text"),
      schema_form_test_schema(),
  }});

  REQUIRE(panel.selected_tool().has_value());
  REQUIRE(panel.findChild<QPlainTextEdit*>(QStringLiteral("toolArguments")) != nullptr);
  REQUIRE(panel.findChild<QWidget*>(QStringLiteral("toolSchemaForm")) != nullptr);
}

TEST_CASE("JSON schema subset parser reads primitive, enum, object, and array fields", "[ui][schema]")
{
  const auto model = ui::parse_json_schema_subset(schema_form_test_schema());

  const auto find_field = [&model](const QString& name) -> const ui::SchemaFormField* {
    for (const auto& field : model.fields) {
      if (field.name == name) {
        return &field;
      }
    }
    return nullptr;
  };

  REQUIRE(model.valid);
  REQUIRE(model.fields.size() == 5);

  const auto* mode = find_field(QStringLiteral("mode"));
  REQUIRE(mode != nullptr);
  REQUIRE(mode->type == ui::SchemaValueType::Enum);

  const auto* options = find_field(QStringLiteral("options"));
  REQUIRE(options != nullptr);
  REQUIRE(options->type == ui::SchemaValueType::Object);
  REQUIRE(options->children.size() == 1);
  REQUIRE(options->children.front().type == ui::SchemaValueType::Array);

  const auto* text = find_field(QStringLiteral("text"));
  REQUIRE(text != nullptr);
  REQUIRE(text->required);
}

TEST_CASE("SchemaFormWidget generates JSON arguments from form fields", "[ui][schema]")
{
  ensure_qapplication();

  ui::SchemaFormWidget widget;
  widget.set_schema(schema_form_test_schema());

  REQUIRE(widget.is_available());

  widget.findChild<QLineEdit*>(QStringLiteral("schemaField_text"))->setText(QStringLiteral("hello"));
  widget.findChild<QLineEdit*>(QStringLiteral("schemaField_retries"))->setText(QStringLiteral("3"));
  widget.findChild<QCheckBox*>(QStringLiteral("schemaField_dryRun"))->setChecked(true);
  widget.findChild<QComboBox*>(QStringLiteral("schemaField_mode"))->setCurrentText(QStringLiteral("safe"));
  widget.findChild<QPlainTextEdit*>(QStringLiteral("schemaField_options_tags"))
      ->setPlainText(QStringLiteral("[\"alpha\",\"beta\"]"));

  QString error_message;
  const auto arguments = widget.arguments(&error_message);

  REQUIRE(arguments.has_value());
  REQUIRE(error_message.isEmpty());
  REQUIRE(arguments->value(QStringLiteral("text")).toString() == QStringLiteral("hello"));
  REQUIRE(arguments->value(QStringLiteral("retries")).toInt() == 3);
  REQUIRE(arguments->value(QStringLiteral("dryRun")).toBool());
  REQUIRE(arguments->value(QStringLiteral("mode")).toString() == QStringLiteral("safe"));

  const auto options = arguments->value(QStringLiteral("options")).toObject();
  REQUIRE(options.value(QStringLiteral("tags")).toArray().size() == 2);
}

TEST_CASE("SchemaFormWidget validates required fields", "[ui][schema]")
{
  ensure_qapplication();

  ui::SchemaFormWidget widget;
  widget.set_schema(schema_form_test_schema());

  QString error_message;
  const auto arguments = widget.arguments(&error_message);

  REQUIRE(!arguments.has_value());
  REQUIRE(error_message.contains(QStringLiteral("text")));
}

TEST_CASE("SchemaFormWidget reports invalid schema fallback state", "[ui][schema]")
{
  ensure_qapplication();

  ui::SchemaFormWidget widget;
  widget.set_schema(QJsonObject{
      {QStringLiteral("type"), QStringLiteral("object")},
      {QStringLiteral("properties"), QJsonObject{
          {QStringLiteral("payload"), QJsonObject{
              {QStringLiteral("type"), QStringLiteral("unsupported")},
          }},
      }},
  });

  REQUIRE(!widget.is_available());
  REQUIRE(widget.error_message().contains(QStringLiteral("payload")));
}
