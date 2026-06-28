#include "McpError.hpp"

#include <QJsonDocument>
#include <QStringList>

namespace mcp {

McpErrorCategory category_for_error_code(int code)
{
  switch (code) {
    case -32700:
      return McpErrorCategory::Parse;
    case -32600:
      return McpErrorCategory::InvalidRequest;
    case -32601:
      return McpErrorCategory::MethodNotFound;
    case -32602:
      return McpErrorCategory::InvalidParams;
    case -32603:
      return McpErrorCategory::Internal;
    default:
      break;
  }

  if (code >= -32099 && code <= -32000) {
    return McpErrorCategory::Server;
  }

  return McpErrorCategory::Unknown;
}

McpErrorSeverity severity_for_category(McpErrorCategory category)
{
  switch (category) {
    case McpErrorCategory::Parse:
    case McpErrorCategory::InvalidRequest:
    case McpErrorCategory::Internal:
    case McpErrorCategory::Transport:
      return McpErrorSeverity::Critical;
    case McpErrorCategory::MethodNotFound:
    case McpErrorCategory::InvalidParams:
    case McpErrorCategory::Server:
      return McpErrorSeverity::Error;
    case McpErrorCategory::Unknown:
      return McpErrorSeverity::Warning;
  }

  return McpErrorSeverity::Error;
}

QString category_name(McpErrorCategory category)
{
  switch (category) {
    case McpErrorCategory::Parse:
      return QStringLiteral("parse");
    case McpErrorCategory::InvalidRequest:
      return QStringLiteral("invalid_request");
    case McpErrorCategory::MethodNotFound:
      return QStringLiteral("method_not_found");
    case McpErrorCategory::InvalidParams:
      return QStringLiteral("invalid_params");
    case McpErrorCategory::Internal:
      return QStringLiteral("internal");
    case McpErrorCategory::Server:
      return QStringLiteral("server");
    case McpErrorCategory::Transport:
      return QStringLiteral("transport");
    case McpErrorCategory::Unknown:
      return QStringLiteral("unknown");
  }

  return QStringLiteral("unknown");
}

QString severity_name(McpErrorSeverity severity)
{
  switch (severity) {
    case McpErrorSeverity::Info:
      return QStringLiteral("info");
    case McpErrorSeverity::Warning:
      return QStringLiteral("warning");
    case McpErrorSeverity::Error:
      return QStringLiteral("error");
    case McpErrorSeverity::Critical:
      return QStringLiteral("critical");
  }

  return QStringLiteral("error");
}

QString title_for_error_code(int code)
{
  switch (code) {
    case -32700:
      return QStringLiteral("Parse error");
    case -32600:
      return QStringLiteral("Invalid request");
    case -32601:
      return QStringLiteral("Method not found");
    case -32602:
      return QStringLiteral("Invalid params");
    case -32603:
      return QStringLiteral("Internal error");
    default:
      break;
  }

  if (code >= -32099 && code <= -32000) {
    return QStringLiteral("Server error");
  }

  return QStringLiteral("Unknown MCP error");
}

QString recommendation_for_category(McpErrorCategory category)
{
  switch (category) {
    case McpErrorCategory::Parse:
      return QStringLiteral("Inspect the raw payload and confirm the MCP server is emitting valid JSON-RPC 2.0 JSON.");
    case McpErrorCategory::InvalidRequest:
      return QStringLiteral("Check that the request contains jsonrpc, id, method, and a valid parameter shape.");
    case McpErrorCategory::MethodNotFound:
      return QStringLiteral("Verify the MCP method name and that the server supports the requested capability.");
    case McpErrorCategory::InvalidParams:
      return QStringLiteral("Compare the request parameters with the server schema and required fields.");
    case McpErrorCategory::Internal:
      return QStringLiteral("Review server stderr and logs; retry only after confirming the server state is healthy.");
    case McpErrorCategory::Server:
      return QStringLiteral("Inspect server-specific error data and stderr for implementation-specific details.");
    case McpErrorCategory::Transport:
      return QStringLiteral("Check process, stdio, network, or session state before retrying the request.");
    case McpErrorCategory::Unknown:
      return QStringLiteral("Inspect the raw error code, message, and data because this code is not part of the known JSON-RPC range.");
  }

  return QStringLiteral("Inspect the raw error details.");
}

bool is_retryable_error_code(int code)
{
  const auto category = category_for_error_code(code);
  return category == McpErrorCategory::Server || category == McpErrorCategory::Internal;
}

McpErrorDescriptor describe_error(const JsonRpcError& error)
{
  const auto category = category_for_error_code(error.code);

  QStringList detail_parts;
  detail_parts << QStringLiteral("Code: %1").arg(error.code);
  if (!error.message.isEmpty()) {
    detail_parts << QStringLiteral("Message: %1").arg(error.message);
  }
  if (error.data.has_value()) {
    const auto serialized_data = QJsonDocument::fromVariant(error.data->toVariant()).toJson(QJsonDocument::Compact);
    detail_parts << QStringLiteral("Data: %1").arg(QString::fromUtf8(serialized_data));
  }

  McpErrorDescriptor descriptor;
  descriptor.category = category;
  descriptor.severity = severity_for_category(category);
  descriptor.title = title_for_error_code(error.code);
  descriptor.detail = detail_parts.join(QStringLiteral("\n"));
  descriptor.recommendation = recommendation_for_category(category);
  descriptor.retryable = is_retryable_error_code(error.code);
  return descriptor;
}

McpErrorDescriptor describe_error(const JsonRpcResponse& response)
{
  if (response.error.has_value()) {
    return describe_error(*response.error);
  }

  McpErrorDescriptor descriptor;
  descriptor.category = McpErrorCategory::Unknown;
  descriptor.severity = McpErrorSeverity::Info;
  descriptor.title = QStringLiteral("No MCP error");
  descriptor.detail = QStringLiteral("The JSON-RPC response does not contain an error object.");
  descriptor.recommendation = QStringLiteral("No action is required.");
  descriptor.retryable = false;
  return descriptor;
}

} // namespace mcp
