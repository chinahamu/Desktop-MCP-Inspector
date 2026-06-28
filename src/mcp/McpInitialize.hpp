#pragma once

#include "McpClient.hpp"
#include "McpTypes.hpp"

#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include <optional>
#include <variant>

namespace mcp {

inline constexpr auto kMcpInitializeMethod = "initialize";
inline constexpr auto kMcpInitializedNotificationMethod = "notifications/initialized";

struct McpInitializeParams
{
  McpProtocolVersion protocol_version = default_protocol_version();
  McpClientInfo client_info;
  QJsonObject capabilities;
};

struct McpInitializeResult
{
  McpProtocolVersion protocol_version;
  McpServerInfo server_info;
  QJsonObject capabilities;
  std::optional<QString> instructions;
};

enum class McpInitializeParseErrorCode {
  ResponseError,
  MissingResult,
  InvalidResult,
  InvalidProtocolVersion,
  InvalidCapabilities,
  InvalidServerInfo,
};

struct McpInitializeParseError
{
  McpInitializeParseErrorCode code = McpInitializeParseErrorCode::InvalidResult;
  QString message;
};

using McpInitializeParseResult = std::variant<McpInitializeResult, McpInitializeParseError>;

[[nodiscard]] QJsonObject to_json_object(const McpInitializeParams& params);
[[nodiscard]] JsonRpcRequest make_initialize_request(
    PendingRequestStore& pending_requests,
    const McpInitializeParams& params);
[[nodiscard]] JsonRpcRequest make_initialize_request(
    PendingRequestStore& pending_requests,
    McpClientInfo client_info,
    QJsonObject capabilities = {});

[[nodiscard]] JsonRpcNotification make_initialized_notification();
[[nodiscard]] bool is_initialized_notification(const JsonRpcNotification& notification);

[[nodiscard]] McpInitializeParseResult parse_initialize_response(const JsonRpcResponse& response);
[[nodiscard]] McpInitializeParseResult parse_initialize_result(const QJsonObject& result);

} // namespace mcp
