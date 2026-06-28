#include "McpTool.hpp"

#include <QJsonValue>

#include <stdexcept>
#include <utility>

namespace mcp {
namespace {

[[nodiscard]] McpToolParseError make_error(McpToolParseErrorCode code, QString message)
{
  return {code, std::move(message)};
}

[[nodiscard]] bool is_present_string(const QJsonValue& value)
{
  return !value.isUndefined() && value.isString();
}

} // namespace

bool is_valid_tool(const McpTool& tool)
{
  return !tool.name.trimmed().isEmpty();
}

QJsonObject to_json_object(const McpTool& tool)
{
  QJsonObject object;
  object.insert(QStringLiteral("name"), tool.name);
  if (tool.description.has_value()) {
    object.insert(QStringLiteral("description"), *tool.description);
  }
  object.insert(QStringLiteral("inputSchema"), tool.input_schema);
  return object;
}

QJsonObject to_json_object(const McpToolsListResult& result)
{
  QJsonArray tools;
  for (const auto& tool : result.tools) {
    tools.append(to_json_object(tool));
  }

  QJsonObject object;
  object.insert(QStringLiteral("tools"), tools);
  if (result.next_cursor.has_value()) {
    object.insert(QStringLiteral("nextCursor"), *result.next_cursor);
  }
  return object;
}

QJsonObject to_json_object(const McpToolCallResult& result)
{
  QJsonObject object;
  object.insert(QStringLiteral("content"), result.content);
  if (result.is_error) {
    object.insert(QStringLiteral("isError"), true);
  }
  if (result.structured_content.has_value()) {
    object.insert(QStringLiteral("structuredContent"), *result.structured_content);
  }
  return object;
}

std::optional<McpTool> tool_from_json_object(const QJsonObject& object)
{
  const auto name_value = object.value(QStringLiteral("name"));
  if (!name_value.isString() || name_value.toString().trimmed().isEmpty()) {
    return std::nullopt;
  }

  const auto input_schema_value = object.value(QStringLiteral("inputSchema"));
  if (!input_schema_value.isObject()) {
    return std::nullopt;
  }

  McpTool tool;
  tool.name = name_value.toString();
  tool.input_schema = input_schema_value.toObject();

  const auto description_value = object.value(QStringLiteral("description"));
  if (!description_value.isUndefined()) {
    if (!description_value.isString()) {
      return std::nullopt;
    }
    tool.description = description_value.toString();
  }

  if (!is_valid_tool(tool)) {
    return std::nullopt;
  }

  return tool;
}

JsonRpcRequest make_tools_list_request(PendingRequestStore& pending_requests, std::optional<QString> cursor)
{
  if (!cursor.has_value() || cursor->isEmpty()) {
    return pending_requests.make_request(QString::fromUtf8(kMcpToolsListMethod));
  }

  return pending_requests.make_request(
      QString::fromUtf8(kMcpToolsListMethod),
      QJsonValue{QJsonObject{{QStringLiteral("cursor"), *cursor}}});
}

JsonRpcRequest make_tools_call_request(PendingRequestStore& pending_requests, QString name, QJsonObject arguments)
{
  if (name.trimmed().isEmpty()) {
    throw std::invalid_argument{"tools/call requires a non-empty tool name"};
  }

  const QJsonObject params{
      {QStringLiteral("name"), name},
      {QStringLiteral("arguments"), arguments},
  };

  return pending_requests.make_request(QString::fromUtf8(kMcpToolsCallMethod), QJsonValue{params});
}

McpToolsListParseResult parse_tools_list_response(const JsonRpcResponse& response)
{
  if (response.error.has_value()) {
    return make_error(McpToolParseErrorCode::ResponseError, response.error->message);
  }

  if (!response.result.has_value()) {
    return make_error(McpToolParseErrorCode::MissingResult, QStringLiteral("tools/list response does not contain a result"));
  }

  if (!response.result->isObject()) {
    return make_error(McpToolParseErrorCode::InvalidResult, QStringLiteral("tools/list result must be a JSON object"));
  }

  return parse_tools_list_result(response.result->toObject());
}

McpToolsListParseResult parse_tools_list_result(const QJsonObject& result)
{
  const auto tools_value = result.value(QStringLiteral("tools"));
  if (!tools_value.isArray()) {
    return make_error(McpToolParseErrorCode::InvalidTools, QStringLiteral("tools/list result tools must be an array"));
  }

  McpToolsListResult list_result;
  const auto tools = tools_value.toArray();
  list_result.tools.reserve(static_cast<std::size_t>(tools.size()));
  for (const auto& value : tools) {
    if (!value.isObject()) {
      return make_error(McpToolParseErrorCode::InvalidTool, QStringLiteral("tools/list entries must be objects"));
    }

    auto tool = tool_from_json_object(value.toObject());
    if (!tool.has_value()) {
      return make_error(McpToolParseErrorCode::InvalidTool, QStringLiteral("tools/list entry must contain name and inputSchema"));
    }

    list_result.tools.push_back(std::move(*tool));
  }

  const auto cursor_value = result.value(QStringLiteral("nextCursor"));
  if (!cursor_value.isUndefined()) {
    if (!is_present_string(cursor_value)) {
      return make_error(McpToolParseErrorCode::InvalidCursor, QStringLiteral("tools/list nextCursor must be a string when present"));
    }
    list_result.next_cursor = cursor_value.toString();
  }

  return list_result;
}

McpToolCallParseResult parse_tools_call_response(const JsonRpcResponse& response)
{
  if (response.error.has_value()) {
    return make_error(McpToolParseErrorCode::ResponseError, response.error->message);
  }

  if (!response.result.has_value()) {
    return make_error(McpToolParseErrorCode::MissingResult, QStringLiteral("tools/call response does not contain a result"));
  }

  if (!response.result->isObject()) {
    return make_error(McpToolParseErrorCode::InvalidResult, QStringLiteral("tools/call result must be a JSON object"));
  }

  return parse_tools_call_result(response.result->toObject());
}

McpToolCallParseResult parse_tools_call_result(const QJsonObject& result)
{
  const auto content_value = result.value(QStringLiteral("content"));
  if (!content_value.isArray()) {
    return make_error(McpToolParseErrorCode::InvalidContent, QStringLiteral("tools/call result content must be an array"));
  }

  McpToolCallResult call_result;
  call_result.content = content_value.toArray();
  call_result.raw_result = result;

  const auto is_error_value = result.value(QStringLiteral("isError"));
  if (!is_error_value.isUndefined()) {
    if (!is_error_value.isBool()) {
      return make_error(McpToolParseErrorCode::InvalidResult, QStringLiteral("tools/call result isError must be a boolean when present"));
    }
    call_result.is_error = is_error_value.toBool();
  }

  const auto structured_content_value = result.value(QStringLiteral("structuredContent"));
  if (!structured_content_value.isUndefined()) {
    if (!structured_content_value.isObject()) {
      return make_error(McpToolParseErrorCode::InvalidResult, QStringLiteral("tools/call result structuredContent must be an object when present"));
    }
    call_result.structured_content = structured_content_value.toObject();
  }

  return call_result;
}

} // namespace mcp
