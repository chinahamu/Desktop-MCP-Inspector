#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QtTypes>

#include <cstddef>
#include <optional>
#include <variant>

namespace mcp {

inline constexpr auto kJsonRpcVersion = "2.0";
inline constexpr auto kDefaultMcpProtocolVersion = "2025-06-18";
inline constexpr auto kDesktopMcpInspectorClientName = "Desktop MCP Inspector";

struct McpProtocolVersion
{
  QString value = QString::fromUtf8(kDefaultMcpProtocolVersion);

  friend bool operator==(const McpProtocolVersion& lhs, const McpProtocolVersion& rhs) = default;
};

struct McpImplementationInfo
{
  QString name;
  QString version;

  friend bool operator==(const McpImplementationInfo& lhs, const McpImplementationInfo& rhs) = default;
};

using McpClientInfo = McpImplementationInfo;
using McpServerInfo = McpImplementationInfo;

[[nodiscard]] McpProtocolVersion default_protocol_version();
[[nodiscard]] McpClientInfo default_client_info(QString version);
[[nodiscard]] bool is_valid_protocol_version(const McpProtocolVersion& protocol_version);
[[nodiscard]] bool is_valid_implementation_info(const McpImplementationInfo& implementation_info);
[[nodiscard]] QJsonObject to_json_object(const McpImplementationInfo& implementation_info);
[[nodiscard]] std::optional<McpImplementationInfo> implementation_info_from_json_object(const QJsonObject& object);

enum class JsonRpcMessageKind {
  Request,
  Response,
  Notification,
};

class RequestId
{
public:
  using Value = std::variant<std::nullptr_t, qint64, QString>;

  RequestId() = default;
  explicit RequestId(Value value);

  [[nodiscard]] static RequestId null();
  [[nodiscard]] static RequestId number(qint64 value);
  [[nodiscard]] static RequestId string(QString value);

  [[nodiscard]] bool is_null() const;
  [[nodiscard]] bool is_number() const;
  [[nodiscard]] bool is_string() const;
  [[nodiscard]] const Value& value() const;
  [[nodiscard]] QJsonValue to_json_value() const;

  friend bool operator==(const RequestId& lhs, const RequestId& rhs) = default;

private:
  Value value_{nullptr};
};

struct JsonRpcError
{
  int code = 0;
  QString message;
  std::optional<QJsonValue> data;
};

struct JsonRpcRequest
{
  RequestId id;
  QString method;
  std::optional<QJsonValue> params;
};

struct JsonRpcNotification
{
  QString method;
  std::optional<QJsonValue> params;
};

struct JsonRpcResponse
{
  RequestId id;
  std::optional<QJsonValue> result;
  std::optional<JsonRpcError> error;

  [[nodiscard]] bool is_success() const;
  [[nodiscard]] bool is_error() const;
};

using JsonRpcMessage = std::variant<JsonRpcRequest, JsonRpcResponse, JsonRpcNotification>;

[[nodiscard]] JsonRpcMessageKind message_kind(const JsonRpcMessage& message);
[[nodiscard]] QString message_kind_name(JsonRpcMessageKind kind);

} // namespace mcp
