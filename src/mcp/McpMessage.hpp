#pragma once

#include "McpTypes.hpp"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include <optional>
#include <variant>

namespace mcp {

enum class JsonRpcParseErrorCode {
  EmptyPayload,
  InvalidJson,
  ExpectedObject,
  InvalidVersion,
  InvalidMethod,
  InvalidId,
  InvalidParams,
  InvalidResponse,
  InvalidError,
};

struct JsonRpcParseError
{
  JsonRpcParseErrorCode code = JsonRpcParseErrorCode::InvalidJson;
  QString message;
  std::optional<int> offset;
};

using JsonRpcParseResult = std::variant<JsonRpcMessage, JsonRpcParseError>;

[[nodiscard]] JsonRpcParseResult parse_json_rpc_message(const QByteArray& payload);
[[nodiscard]] JsonRpcParseResult parse_json_rpc_message(const QJsonObject& object);

[[nodiscard]] QJsonObject to_json_object(const JsonRpcError& error);
[[nodiscard]] QJsonObject to_json_object(const JsonRpcRequest& request);
[[nodiscard]] QJsonObject to_json_object(const JsonRpcNotification& notification);
[[nodiscard]] QJsonObject to_json_object(const JsonRpcResponse& response);
[[nodiscard]] QJsonObject to_json_object(const JsonRpcMessage& message);

[[nodiscard]] QByteArray serialize_json_rpc_message(
    const JsonRpcMessage& message,
    QJsonDocument::JsonFormat format = QJsonDocument::Compact);

} // namespace mcp
