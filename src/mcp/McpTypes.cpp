#include "McpTypes.hpp"

#include <utility>

namespace mcp {
namespace {

template <class... Ts>
struct Overloaded : Ts...
{
  using Ts::operator()...;
};

template <class... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

} // namespace

McpProtocolVersion default_protocol_version()
{
  return McpProtocolVersion{QString::fromUtf8(kDefaultMcpProtocolVersion)};
}

McpClientInfo default_client_info(QString version)
{
  return McpClientInfo{QString::fromUtf8(kDesktopMcpInspectorClientName), std::move(version)};
}

bool is_valid_protocol_version(const McpProtocolVersion& protocol_version)
{
  return !protocol_version.value.trimmed().isEmpty();
}

bool is_valid_implementation_info(const McpImplementationInfo& implementation_info)
{
  return !implementation_info.name.trimmed().isEmpty() && !implementation_info.version.trimmed().isEmpty();
}

QJsonObject to_json_object(const McpImplementationInfo& implementation_info)
{
  QJsonObject object;
  object.insert(QStringLiteral("name"), implementation_info.name);
  object.insert(QStringLiteral("version"), implementation_info.version);
  return object;
}

std::optional<McpImplementationInfo> implementation_info_from_json_object(const QJsonObject& object)
{
  const auto name = object.value(QStringLiteral("name"));
  const auto version = object.value(QStringLiteral("version"));
  if (!name.isString() || !version.isString()) {
    return std::nullopt;
  }

  McpImplementationInfo implementation_info;
  implementation_info.name = name.toString();
  implementation_info.version = version.toString();
  if (!is_valid_implementation_info(implementation_info)) {
    return std::nullopt;
  }

  return implementation_info;
}

RequestId::RequestId(Value value)
  : value_(std::move(value))
{
}

RequestId RequestId::null()
{
  return RequestId{Value{nullptr}};
}

RequestId RequestId::number(qint64 value)
{
  return RequestId{Value{value}};
}

RequestId RequestId::string(QString value)
{
  return RequestId{Value{std::move(value)}};
}

bool RequestId::is_null() const
{
  return std::holds_alternative<std::nullptr_t>(value_);
}

bool RequestId::is_number() const
{
  return std::holds_alternative<qint64>(value_);
}

bool RequestId::is_string() const
{
  return std::holds_alternative<QString>(value_);
}

const RequestId::Value& RequestId::value() const
{
  return value_;
}

QJsonValue RequestId::to_json_value() const
{
  return std::visit(
      Overloaded{
          [](std::nullptr_t) { return QJsonValue{QJsonValue::Null}; },
          [](qint64 value) { return QJsonValue{static_cast<double>(value)}; },
          [](const QString& value) { return QJsonValue{value}; },
      },
      value_);
}

bool JsonRpcResponse::is_success() const
{
  return result.has_value() && !error.has_value();
}

bool JsonRpcResponse::is_error() const
{
  return error.has_value();
}

JsonRpcMessageKind message_kind(const JsonRpcMessage& message)
{
  return std::visit(
      Overloaded{
          [](const JsonRpcRequest&) { return JsonRpcMessageKind::Request; },
          [](const JsonRpcResponse&) { return JsonRpcMessageKind::Response; },
          [](const JsonRpcNotification&) { return JsonRpcMessageKind::Notification; },
      },
      message);
}

QString message_kind_name(JsonRpcMessageKind kind)
{
  switch (kind) {
    case JsonRpcMessageKind::Request:
      return QStringLiteral("request");
    case JsonRpcMessageKind::Response:
      return QStringLiteral("response");
    case JsonRpcMessageKind::Notification:
      return QStringLiteral("notification");
  }

  return QStringLiteral("unknown");
}

} // namespace mcp
