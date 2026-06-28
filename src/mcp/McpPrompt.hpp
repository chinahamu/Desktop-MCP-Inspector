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

inline constexpr auto kMcpPromptsListMethod = "prompts/list";
inline constexpr auto kMcpPromptsGetMethod = "prompts/get";

struct McpPromptArgument
{
  QString name;
  std::optional<QString> description;
  bool required = false;

  friend bool operator==(const McpPromptArgument& lhs, const McpPromptArgument& rhs) = default;
};

struct McpPrompt
{
  QString name;
  std::optional<QString> description;
  std::vector<McpPromptArgument> arguments;

  friend bool operator==(const McpPrompt& lhs, const McpPrompt& rhs) = default;
};

struct McpPromptsListResult
{
  std::vector<McpPrompt> prompts;
  std::optional<QString> next_cursor;
};

struct McpPromptGetResult
{
  std::optional<QString> description;
  QJsonArray messages;
  QJsonObject raw_result;
};

enum class McpPromptParseErrorCode {
  ResponseError,
  MissingResult,
  InvalidResult,
  InvalidPrompt,
  InvalidPrompts,
  InvalidArgument,
  InvalidArguments,
  InvalidCursor,
  InvalidMessages,
};

struct McpPromptParseError
{
  McpPromptParseErrorCode code = McpPromptParseErrorCode::InvalidResult;
  QString message;
};

using McpPromptsListParseResult = std::variant<McpPromptsListResult, McpPromptParseError>;
using McpPromptGetParseResult = std::variant<McpPromptGetResult, McpPromptParseError>;

[[nodiscard]] bool is_valid_prompt_argument(const McpPromptArgument& argument);
[[nodiscard]] bool is_valid_prompt(const McpPrompt& prompt);
[[nodiscard]] QJsonObject to_json_object(const McpPromptArgument& argument);
[[nodiscard]] QJsonObject to_json_object(const McpPrompt& prompt);
[[nodiscard]] QJsonObject to_json_object(const McpPromptsListResult& result);
[[nodiscard]] QJsonObject to_json_object(const McpPromptGetResult& result);
[[nodiscard]] std::optional<McpPromptArgument> prompt_argument_from_json_object(const QJsonObject& object);
[[nodiscard]] std::optional<McpPrompt> prompt_from_json_object(const QJsonObject& object);

[[nodiscard]] JsonRpcRequest make_prompts_list_request(
    PendingRequestStore& pending_requests,
    std::optional<QString> cursor = std::nullopt);
[[nodiscard]] JsonRpcRequest make_prompts_get_request(
    PendingRequestStore& pending_requests,
    QString name,
    QJsonObject arguments = {});

[[nodiscard]] McpPromptsListParseResult parse_prompts_list_response(const JsonRpcResponse& response);
[[nodiscard]] McpPromptsListParseResult parse_prompts_list_result(const QJsonObject& result);
[[nodiscard]] McpPromptGetParseResult parse_prompts_get_response(const JsonRpcResponse& response);
[[nodiscard]] McpPromptGetParseResult parse_prompts_get_result(const QJsonObject& result);

} // namespace mcp
