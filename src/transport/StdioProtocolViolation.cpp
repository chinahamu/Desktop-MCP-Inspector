#include "StdioProtocolViolation.hpp"

#include "McpMessage.hpp"

#include <QJsonDocument>

#include <variant>

namespace transport {
namespace {

[[nodiscard]] QString parse_error_code_name(mcp::JsonRpcParseErrorCode code)
{
  switch (code) {
  case mcp::JsonRpcParseErrorCode::EmptyPayload:
    return QStringLiteral("empty_payload");
  case mcp::JsonRpcParseErrorCode::InvalidJson:
    return QStringLiteral("invalid_json");
  case mcp::JsonRpcParseErrorCode::ExpectedObject:
    return QStringLiteral("expected_object");
  case mcp::JsonRpcParseErrorCode::InvalidVersion:
    return QStringLiteral("invalid_version");
  case mcp::JsonRpcParseErrorCode::InvalidMethod:
    return QStringLiteral("invalid_method");
  case mcp::JsonRpcParseErrorCode::InvalidId:
    return QStringLiteral("invalid_id");
  case mcp::JsonRpcParseErrorCode::InvalidParams:
    return QStringLiteral("invalid_params");
  case mcp::JsonRpcParseErrorCode::InvalidResponse:
    return QStringLiteral("invalid_response");
  case mcp::JsonRpcParseErrorCode::InvalidError:
    return QStringLiteral("invalid_error");
  }

  return QStringLiteral("unknown_parse_error");
}

[[nodiscard]] QString compact_json(const QJsonObject& object)
{
  return QString::fromUtf8(QJsonDocument{object}.toJson(QJsonDocument::Compact));
}

} // namespace

std::optional<StdioProtocolViolation> detect_stdio_protocol_violation(const QJsonObject& message)
{
  const auto parsed = mcp::parse_json_rpc_message(message);
  if (std::holds_alternative<mcp::JsonRpcMessage>(parsed)) {
    return std::nullopt;
  }

  const auto& error = std::get<mcp::JsonRpcParseError>(parsed);
  return StdioProtocolViolation{
      .code = parse_error_code_name(error.code),
      .message = error.message,
      .details = QJsonObject{
          {QStringLiteral("code"), parse_error_code_name(error.code)},
          {QStringLiteral("message"), error.message},
          {QStringLiteral("raw"), compact_json(message)},
      },
  };
}

} // namespace transport
