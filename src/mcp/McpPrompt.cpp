#include "McpPrompt.hpp"

#include <QJsonValue>

#include <stdexcept>
#include <utility>

namespace mcp {
namespace {

[[nodiscard]] McpPromptParseError make_error(McpPromptParseErrorCode code, QString message)
{
  return {code, std::move(message)};
}

[[nodiscard]] bool is_present_string(const QJsonValue& value)
{
  return !value.isUndefined() && value.isString();
}

} // namespace

bool is_valid_prompt_argument(const McpPromptArgument& argument)
{
  return !argument.name.trimmed().isEmpty();
}

bool is_valid_prompt(const McpPrompt& prompt)
{
  return !prompt.name.trimmed().isEmpty();
}

QJsonObject to_json_object(const McpPromptArgument& argument)
{
  QJsonObject object;
  object.insert(QStringLiteral("name"), argument.name);
  if (argument.description.has_value()) {
    object.insert(QStringLiteral("description"), *argument.description);
  }
  if (argument.required) {
    object.insert(QStringLiteral("required"), true);
  }
  return object;
}

QJsonObject to_json_object(const McpPrompt& prompt)
{
  QJsonObject object;
  object.insert(QStringLiteral("name"), prompt.name);
  if (prompt.description.has_value()) {
    object.insert(QStringLiteral("description"), *prompt.description);
  }

  if (!prompt.arguments.empty()) {
    QJsonArray arguments;
    for (const auto& argument : prompt.arguments) {
      arguments.append(to_json_object(argument));
    }
    object.insert(QStringLiteral("arguments"), arguments);
  }
  return object;
}

QJsonObject to_json_object(const McpPromptsListResult& result)
{
  QJsonArray prompts;
  for (const auto& prompt : result.prompts) {
    prompts.append(to_json_object(prompt));
  }

  QJsonObject object;
  object.insert(QStringLiteral("prompts"), prompts);
  if (result.next_cursor.has_value()) {
    object.insert(QStringLiteral("nextCursor"), *result.next_cursor);
  }
  return object;
}

QJsonObject to_json_object(const McpPromptGetResult& result)
{
  QJsonObject object = result.raw_result;
  if (result.description.has_value()) {
    object.insert(QStringLiteral("description"), *result.description);
  }
  object.insert(QStringLiteral("messages"), result.messages);
  return object;
}

std::optional<McpPromptArgument> prompt_argument_from_json_object(const QJsonObject& object)
{
  const auto name_value = object.value(QStringLiteral("name"));
  if (!name_value.isString() || name_value.toString().trimmed().isEmpty()) {
    return std::nullopt;
  }

  McpPromptArgument argument;
  argument.name = name_value.toString();

  const auto description_value = object.value(QStringLiteral("description"));
  if (!description_value.isUndefined()) {
    if (!description_value.isString()) {
      return std::nullopt;
    }
    argument.description = description_value.toString();
  }

  const auto required_value = object.value(QStringLiteral("required"));
  if (!required_value.isUndefined()) {
    if (!required_value.isBool()) {
      return std::nullopt;
    }
    argument.required = required_value.toBool();
  }

  if (!is_valid_prompt_argument(argument)) {
    return std::nullopt;
  }

  return argument;
}

std::optional<McpPrompt> prompt_from_json_object(const QJsonObject& object)
{
  const auto name_value = object.value(QStringLiteral("name"));
  if (!name_value.isString() || name_value.toString().trimmed().isEmpty()) {
    return std::nullopt;
  }

  McpPrompt prompt;
  prompt.name = name_value.toString();

  const auto description_value = object.value(QStringLiteral("description"));
  if (!description_value.isUndefined()) {
    if (!description_value.isString()) {
      return std::nullopt;
    }
    prompt.description = description_value.toString();
  }

  const auto arguments_value = object.value(QStringLiteral("arguments"));
  if (!arguments_value.isUndefined()) {
    if (!arguments_value.isArray()) {
      return std::nullopt;
    }

    const auto arguments = arguments_value.toArray();
    prompt.arguments.reserve(static_cast<std::size_t>(arguments.size()));
    for (const auto& value : arguments) {
      if (!value.isObject()) {
        return std::nullopt;
      }

      auto argument = prompt_argument_from_json_object(value.toObject());
      if (!argument.has_value()) {
        return std::nullopt;
      }
      prompt.arguments.push_back(std::move(*argument));
    }
  }

  if (!is_valid_prompt(prompt)) {
    return std::nullopt;
  }

  return prompt;
}

JsonRpcRequest make_prompts_list_request(PendingRequestStore& pending_requests, std::optional<QString> cursor)
{
  if (!cursor.has_value() || cursor->isEmpty()) {
    return pending_requests.make_request(QString::fromUtf8(kMcpPromptsListMethod));
  }

  return pending_requests.make_request(
      QString::fromUtf8(kMcpPromptsListMethod),
      QJsonValue{QJsonObject{{QStringLiteral("cursor"), *cursor}}});
}

JsonRpcRequest make_prompts_get_request(PendingRequestStore& pending_requests, QString name, QJsonObject arguments)
{
  if (name.trimmed().isEmpty()) {
    throw std::invalid_argument{"prompts/get requires a non-empty prompt name"};
  }

  const QJsonObject params{
      {QStringLiteral("name"), std::move(name)},
      {QStringLiteral("arguments"), arguments},
  };

  return pending_requests.make_request(QString::fromUtf8(kMcpPromptsGetMethod), QJsonValue{params});
}

McpPromptsListParseResult parse_prompts_list_response(const JsonRpcResponse& response)
{
  if (response.error.has_value()) {
    return make_error(McpPromptParseErrorCode::ResponseError, response.error->message);
  }

  if (!response.result.has_value()) {
    return make_error(McpPromptParseErrorCode::MissingResult, QStringLiteral("prompts/list response does not contain a result"));
  }

  if (!response.result->isObject()) {
    return make_error(McpPromptParseErrorCode::InvalidResult, QStringLiteral("prompts/list result must be a JSON object"));
  }

  return parse_prompts_list_result(response.result->toObject());
}

McpPromptsListParseResult parse_prompts_list_result(const QJsonObject& result)
{
  const auto prompts_value = result.value(QStringLiteral("prompts"));
  if (!prompts_value.isArray()) {
    return make_error(McpPromptParseErrorCode::InvalidPrompts, QStringLiteral("prompts/list result prompts must be an array"));
  }

  McpPromptsListResult list_result;
  const auto prompts = prompts_value.toArray();
  list_result.prompts.reserve(static_cast<std::size_t>(prompts.size()));
  for (const auto& value : prompts) {
    if (!value.isObject()) {
      return make_error(McpPromptParseErrorCode::InvalidPrompt, QStringLiteral("prompts/list entries must be objects"));
    }

    auto prompt = prompt_from_json_object(value.toObject());
    if (!prompt.has_value()) {
      return make_error(McpPromptParseErrorCode::InvalidPrompt, QStringLiteral("prompts/list entry must contain a name and valid arguments"));
    }

    list_result.prompts.push_back(std::move(*prompt));
  }

  const auto cursor_value = result.value(QStringLiteral("nextCursor"));
  if (!cursor_value.isUndefined()) {
    if (!is_present_string(cursor_value)) {
      return make_error(McpPromptParseErrorCode::InvalidCursor, QStringLiteral("prompts/list nextCursor must be a string when present"));
    }
    list_result.next_cursor = cursor_value.toString();
  }

  return list_result;
}

McpPromptGetParseResult parse_prompts_get_response(const JsonRpcResponse& response)
{
  if (response.error.has_value()) {
    return make_error(McpPromptParseErrorCode::ResponseError, response.error->message);
  }

  if (!response.result.has_value()) {
    return make_error(McpPromptParseErrorCode::MissingResult, QStringLiteral("prompts/get response does not contain a result"));
  }

  if (!response.result->isObject()) {
    return make_error(McpPromptParseErrorCode::InvalidResult, QStringLiteral("prompts/get result must be a JSON object"));
  }

  return parse_prompts_get_result(response.result->toObject());
}

McpPromptGetParseResult parse_prompts_get_result(const QJsonObject& result)
{
  McpPromptGetResult get_result;
  get_result.raw_result = result;

  const auto description_value = result.value(QStringLiteral("description"));
  if (!description_value.isUndefined()) {
    if (!description_value.isString()) {
      return make_error(McpPromptParseErrorCode::InvalidResult, QStringLiteral("prompts/get description must be a string when present"));
    }
    get_result.description = description_value.toString();
  }

  const auto messages_value = result.value(QStringLiteral("messages"));
  if (!messages_value.isArray()) {
    return make_error(McpPromptParseErrorCode::InvalidMessages, QStringLiteral("prompts/get result messages must be an array"));
  }

  const auto messages = messages_value.toArray();
  for (const auto& value : messages) {
    if (!value.isObject()) {
      return make_error(McpPromptParseErrorCode::InvalidMessages, QStringLiteral("prompts/get message entries must be objects"));
    }
  }

  get_result.messages = messages;
  return get_result;
}

} // namespace mcp
