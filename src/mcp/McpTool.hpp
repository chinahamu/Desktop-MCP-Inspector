#pragma once

#include "McpClient.hpp"
#include "McpTypes.hpp"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include <optional>
#include <variant>
#include <vector>

namespace mcp {

inline constexpr auto kMcpToolsListMethod = "tools/list";
inline constexpr auto kMcpToolsCallMethod = "tools/call";

struct McpTool
{
  QString name;
  std::optional<QString> description;
  QJsonObject input_schema;

  friend bool operator==(const McpTool& lhs, const McpTool& rhs) = default;
};

struct McpToolsListResult
{
  std::vector<McpTool> tools;
  std::optional<QString> next_cursor;
};

struct McpToolCallResult
{
  QJsonArray content;
  bool is_error = false;
  std::optional<QJsonObject> structured_content;
  QJsonObject raw_result;
};

enum class McpToolParseErrorCode {
  ResponseError,
  MissingResult,
  InvalidResult,
  InvalidTool,
  InvalidTools,
  InvalidCursor,
  InvalidContent,
};

struct McpToolParseError
{
  McpToolParseErrorCode code = McpToolParseErrorCode::InvalidResult;
  QString message;
};

using McpToolsListParseResult = std::variant<McpToolsListResult, McpToolParseError>;
using McpToolCallParseResult = std::variant<McpToolCallResult, McpToolParseError>;

[[nodiscard]] bool is_valid_tool(const McpTool& tool);
[[nodiscard]] QJsonObject to_json_object(const McpTool& tool);
[[nodiscard]] QJsonObject to_json_object(const McpToolsListResult& result);
[[nodiscard]] QJsonObject to_json_object(const McpToolCallResult& result);
[[nodiscard]] std::optional<McpTool> tool_from_json_object(const QJsonObject& object);

[[nodiscard]] JsonRpcRequest make_tools_list_request(
    PendingRequestStore& pending_requests,
    std::optional<QString> cursor = std::nullopt);
[[nodiscard]] JsonRpcRequest make_tools_call_request(
    PendingRequestStore& pending_requests,
    QString name,
    QJsonObject arguments = {});

[[nodiscard]] McpToolsListParseResult parse_tools_list_response(const JsonRpcResponse& response);
[[nodiscard]] McpToolsListParseResult parse_tools_list_result(const QJsonObject& result);
[[nodiscard]] McpToolCallParseResult parse_tools_call_response(const JsonRpcResponse& response);
[[nodiscard]] McpToolCallParseResult parse_tools_call_result(const QJsonObject& result);

} // namespace mcp
