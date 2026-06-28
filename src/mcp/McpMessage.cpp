#include "McpMessage.hpp"

#include <QJsonParseError>
#include <QJsonValue>

#include <cmath>
#include <limits>
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

[[nodiscard]] JsonRpcParseError make_error(
    JsonRpcParseErrorCode code,
    QString message,
    std::optional<int> offset = std::nullopt)
{
  return {code, std::move(message), offset};
}

[[nodiscard]] bool has_valid_jsonrpc_version(const QJsonObject& object)
{
  const auto value = object.value(QStringLiteral("jsonrpc"));
  return value.isString() && value.toString() == QString::fromUtf8(kJsonRpcVersion);
}

[[nodiscard]] bool is_valid_params(const QJsonValue& value)
{
  return value.isObject() || value.isArray();
}

[[nodiscard]] std::variant<qint64, JsonRpcParseError> parse_integer_id(const QJsonValue& value)
{
  const auto number = value.toDouble();
  if (!std::isfinite(number) || std::trunc(number) != number) {
    return make_error(JsonRpcParseErrorCode::InvalidId, QStringLiteral("JSON-RPC id number must be an integer"));
  }

  if (number < static_cast<double>(std::numeric_limits<qint64>::min())
      || number > static_cast<double>(std::numeric_limits<qint64>::max())) {
    return make_error(JsonRpcParseErrorCode::InvalidId, QStringLiteral("JSON-RPC id number is outside qint64 range"));
  }

  return static_cast<qint64>(number);
}

[[nodiscard]] std::variant<int, JsonRpcParseError> parse_error_code(const QJsonValue& value)
{
  if (!value.isDouble()) {
    return make_error(JsonRpcParseErrorCode::InvalidError, QStringLiteral("JSON-RPC error code must be a number"));
  }

  const auto number = value.toDouble();
  if (!std::isfinite(number) || std::trunc(number) != number) {
    return make_error(JsonRpcParseErrorCode::InvalidError, QStringLiteral("JSON-RPC error code must be an integer"));
  }

  if (number < static_cast<double>(std::numeric_limits<int>::min())
      || number > static_cast<double>(std::numeric_limits<int>::max())) {
    return make_error(JsonRpcParseErrorCode::InvalidError, QStringLiteral("JSON-RPC error code is outside int range"));
  }

  return static_cast<int>(number);
}

[[nodiscard]] std::variant<RequestId, JsonRpcParseError> parse_request_id(const QJsonValue& value)
{
  if (value.isNull()) {
    return RequestId::null();
  }

  if (value.isString()) {
    return RequestId::string(value.toString());
  }

  if (value.isDouble()) {
    auto parsed_number = parse_integer_id(value);
    if (auto* error = std::get_if<JsonRpcParseError>(&parsed_number)) {
      return *error;
    }

    return RequestId::number(std::get<qint64>(parsed_number));
  }

  return make_error(JsonRpcParseErrorCode::InvalidId, QStringLiteral("JSON-RPC id must be string, integer, or null"));
}

[[nodiscard]] std::variant<JsonRpcError, JsonRpcParseError> parse_error_object(const QJsonValue& value)
{
  if (!value.isObject()) {
    return make_error(JsonRpcParseErrorCode::InvalidError, QStringLiteral("JSON-RPC error must be an object"));
  }

  const auto object = value.toObject();
  const auto parsed_code = parse_error_code(object.value(QStringLiteral("code")));
  if (auto* error = std::get_if<JsonRpcParseError>(&parsed_code)) {
    return *error;
  }

  const auto message_value = object.value(QStringLiteral("message"));
  if (!message_value.isString()) {
    return make_error(JsonRpcParseErrorCode::InvalidError, QStringLiteral("JSON-RPC error message must be a string"));
  }

  JsonRpcError error;
  error.code = std::get<int>(parsed_code);
  error.message = message_value.toString();

  if (object.contains(QStringLiteral("data"))) {
    error.data = object.value(QStringLiteral("data"));
  }

  return error;
}

[[nodiscard]] JsonRpcParseResult parse_method_message(const QJsonObject& object)
{
  const auto method_value = object.value(QStringLiteral("method"));
  if (!method_value.isString() || method_value.toString().isEmpty()) {
    return make_error(JsonRpcParseErrorCode::InvalidMethod, QStringLiteral("JSON-RPC method must be a non-empty string"));
  }

  std::optional<QJsonValue> params;
  if (object.contains(QStringLiteral("params"))) {
    const auto params_value = object.value(QStringLiteral("params"));
    if (!is_valid_params(params_value)) {
      return make_error(JsonRpcParseErrorCode::InvalidParams, QStringLiteral("JSON-RPC params must be an object or array"));
    }
    params = params_value;
  }

  if (object.contains(QStringLiteral("id"))) {
    auto parsed_id = parse_request_id(object.value(QStringLiteral("id")));
    if (auto* error = std::get_if<JsonRpcParseError>(&parsed_id)) {
      return *error;
    }

    JsonRpcRequest request;
    request.id = std::get<RequestId>(std::move(parsed_id));
    request.method = method_value.toString();
    request.params = std::move(params);
    return JsonRpcMessage{std::move(request)};
  }

  JsonRpcNotification notification;
  notification.method = method_value.toString();
  notification.params = std::move(params);
  return JsonRpcMessage{std::move(notification)};
}

[[nodiscard]] JsonRpcParseResult parse_response_message(const QJsonObject& object)
{
  if (!object.contains(QStringLiteral("id"))) {
    return make_error(JsonRpcParseErrorCode::InvalidResponse, QStringLiteral("JSON-RPC response must contain an id"));
  }

  auto parsed_id = parse_request_id(object.value(QStringLiteral("id")));
  if (auto* error = std::get_if<JsonRpcParseError>(&parsed_id)) {
    return *error;
  }

  const auto has_result = object.contains(QStringLiteral("result"));
  const auto has_error = object.contains(QStringLiteral("error"));
  if (has_result == has_error) {
    return make_error(JsonRpcParseErrorCode::InvalidResponse, QStringLiteral("JSON-RPC response must contain exactly one of result or error"));
  }

  JsonRpcResponse response;
  response.id = std::get<RequestId>(std::move(parsed_id));

  if (has_error) {
    auto parsed_error = parse_error_object(object.value(QStringLiteral("error")));
    if (auto* error = std::get_if<JsonRpcParseError>(&parsed_error)) {
      return *error;
    }
    response.error = std::get<JsonRpcError>(std::move(parsed_error));
  } else {
    response.result = object.value(QStringLiteral("result"));
  }

  return JsonRpcMessage{std::move(response)};
}

} // namespace

JsonRpcParseResult parse_json_rpc_message(const QByteArray& payload)
{
  if (payload.trimmed().isEmpty()) {
    return make_error(JsonRpcParseErrorCode::EmptyPayload, QStringLiteral("JSON-RPC payload is empty"));
  }

  QJsonParseError parse_error;
  const auto document = QJsonDocument::fromJson(payload, &parse_error);
  if (parse_error.error != QJsonParseError::NoError) {
    return make_error(JsonRpcParseErrorCode::InvalidJson, parse_error.errorString(), parse_error.offset);
  }

  if (!document.isObject()) {
    return make_error(JsonRpcParseErrorCode::ExpectedObject, QStringLiteral("JSON-RPC payload must be a JSON object"));
  }

  return parse_json_rpc_message(document.object());
}

JsonRpcParseResult parse_json_rpc_message(const QJsonObject& object)
{
  if (!has_valid_jsonrpc_version(object)) {
    return make_error(JsonRpcParseErrorCode::InvalidVersion, QStringLiteral("JSON-RPC version must be 2.0"));
  }

  if (object.contains(QStringLiteral("method"))) {
    return parse_method_message(object);
  }

  return parse_response_message(object);
}

QJsonObject to_json_object(const JsonRpcError& error)
{
  QJsonObject object;
  object.insert(QStringLiteral("code"), error.code);
  object.insert(QStringLiteral("message"), error.message);

  if (error.data.has_value()) {
    object.insert(QStringLiteral("data"), *error.data);
  }

  return object;
}

QJsonObject to_json_object(const JsonRpcRequest& request)
{
  QJsonObject object;
  object.insert(QStringLiteral("jsonrpc"), QString::fromUtf8(kJsonRpcVersion));
  object.insert(QStringLiteral("id"), request.id.to_json_value());
  object.insert(QStringLiteral("method"), request.method);

  if (request.params.has_value()) {
    object.insert(QStringLiteral("params"), *request.params);
  }

  return object;
}

QJsonObject to_json_object(const JsonRpcNotification& notification)
{
  QJsonObject object;
  object.insert(QStringLiteral("jsonrpc"), QString::fromUtf8(kJsonRpcVersion));
  object.insert(QStringLiteral("method"), notification.method);

  if (notification.params.has_value()) {
    object.insert(QStringLiteral("params"), *notification.params);
  }

  return object;
}

QJsonObject to_json_object(const JsonRpcResponse& response)
{
  QJsonObject object;
  object.insert(QStringLiteral("jsonrpc"), QString::fromUtf8(kJsonRpcVersion));
  object.insert(QStringLiteral("id"), response.id.to_json_value());

  if (response.error.has_value()) {
    object.insert(QStringLiteral("error"), to_json_object(*response.error));
  } else if (response.result.has_value()) {
    object.insert(QStringLiteral("result"), *response.result);
  } else {
    object.insert(QStringLiteral("result"), QJsonValue{QJsonValue::Null});
  }

  return object;
}

QJsonObject to_json_object(const JsonRpcMessage& message)
{
  return std::visit(
      Overloaded{
          [](const JsonRpcRequest& request) { return to_json_object(request); },
          [](const JsonRpcResponse& response) { return to_json_object(response); },
          [](const JsonRpcNotification& notification) { return to_json_object(notification); },
      },
      message);
}

QByteArray serialize_json_rpc_message(const JsonRpcMessage& message, QJsonDocument::JsonFormat format)
{
  return QJsonDocument{to_json_object(message)}.toJson(format);
}

} // namespace mcp
