#include "McpInitialize.hpp"

#include <QJsonObject>

#include <utility>

namespace mcp {
namespace {

[[nodiscard]] McpInitializeParseError make_error(McpInitializeParseErrorCode code, QString message)
{
  return {code, std::move(message)};
}

} // namespace

QJsonObject to_json_object(const McpInitializeParams& params)
{
  QJsonObject object;
  object.insert(QStringLiteral("protocolVersion"), params.protocol_version.value);
  object.insert(QStringLiteral("capabilities"), params.capabilities);
  object.insert(QStringLiteral("clientInfo"), to_json_object(params.client_info));
  return object;
}

JsonRpcRequest make_initialize_request(PendingRequestStore& pending_requests, const McpInitializeParams& params)
{
  return pending_requests.make_request(
      QString::fromUtf8(kMcpInitializeMethod),
      QJsonValue{to_json_object(params)});
}

JsonRpcRequest make_initialize_request(
    PendingRequestStore& pending_requests,
    McpClientInfo client_info,
    QJsonObject capabilities)
{
  McpInitializeParams params;
  params.protocol_version = default_protocol_version();
  params.client_info = std::move(client_info);
  params.capabilities = std::move(capabilities);
  return make_initialize_request(pending_requests, params);
}

JsonRpcNotification make_initialized_notification()
{
  JsonRpcNotification notification;
  notification.method = QString::fromUtf8(kMcpInitializedNotificationMethod);
  notification.params = std::nullopt;
  return notification;
}

bool is_initialized_notification(const JsonRpcNotification& notification)
{
  return notification.method == QString::fromUtf8(kMcpInitializedNotificationMethod) && !notification.params.has_value();
}

McpInitializeParseResult parse_initialize_response(const JsonRpcResponse& response)
{
  if (response.error.has_value()) {
    return make_error(McpInitializeParseErrorCode::ResponseError, response.error->message);
  }

  if (!response.result.has_value()) {
    return make_error(McpInitializeParseErrorCode::MissingResult, QStringLiteral("initialize response does not contain a result"));
  }

  if (!response.result->isObject()) {
    return make_error(McpInitializeParseErrorCode::InvalidResult, QStringLiteral("initialize result must be a JSON object"));
  }

  return parse_initialize_result(response.result->toObject());
}

McpInitializeParseResult parse_initialize_result(const QJsonObject& result)
{
  const auto protocol_version_value = result.value(QStringLiteral("protocolVersion"));
  if (!protocol_version_value.isString()) {
    return make_error(McpInitializeParseErrorCode::InvalidProtocolVersion, QStringLiteral("initialize result protocolVersion must be a string"));
  }

  McpProtocolVersion protocol_version{protocol_version_value.toString()};
  if (!is_valid_protocol_version(protocol_version)) {
    return make_error(McpInitializeParseErrorCode::InvalidProtocolVersion, QStringLiteral("initialize result protocolVersion must not be blank"));
  }

  const auto capabilities_value = result.value(QStringLiteral("capabilities"));
  if (!capabilities_value.isObject()) {
    return make_error(McpInitializeParseErrorCode::InvalidCapabilities, QStringLiteral("initialize result capabilities must be an object"));
  }

  const auto server_info_value = result.value(QStringLiteral("serverInfo"));
  if (!server_info_value.isObject()) {
    return make_error(McpInitializeParseErrorCode::InvalidServerInfo, QStringLiteral("initialize result serverInfo must be an object"));
  }

  const auto server_info = implementation_info_from_json_object(server_info_value.toObject());
  if (!server_info.has_value()) {
    return make_error(McpInitializeParseErrorCode::InvalidServerInfo, QStringLiteral("initialize result serverInfo must contain non-empty name and version"));
  }

  McpInitializeResult initialize_result;
  initialize_result.protocol_version = std::move(protocol_version);
  initialize_result.capabilities = capabilities_value.toObject();
  initialize_result.server_info = *server_info;

  const auto instructions_value = result.value(QStringLiteral("instructions"));
  if (!instructions_value.isUndefined()) {
    if (!instructions_value.isString()) {
      return make_error(McpInitializeParseErrorCode::InvalidResult, QStringLiteral("initialize result instructions must be a string when present"));
    }
    initialize_result.instructions = instructions_value.toString();
  }

  return initialize_result;
}

} // namespace mcp
