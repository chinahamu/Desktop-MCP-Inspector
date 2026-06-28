#include <catch2/catch_test_macros.hpp>

#include "McpTypes.hpp"

#include <QJsonValue>
#include <QString>

#include <optional>

TEST_CASE("request id supports JSON-RPC number string and null values", "[mcp][types]")
{
  const auto number_id = mcp::RequestId::number(7);
  REQUIRE(number_id.is_number());
  REQUIRE_FALSE(number_id.is_string());
  REQUIRE_FALSE(number_id.is_null());
  REQUIRE(number_id.to_json_value().toInt() == 7);

  const auto string_id = mcp::RequestId::string(QStringLiteral("req-1"));
  REQUIRE(string_id.is_string());
  REQUIRE(string_id.to_json_value().toString() == QStringLiteral("req-1"));

  const auto null_id = mcp::RequestId::null();
  REQUIRE(null_id.is_null());
  REQUIRE(null_id.to_json_value().isNull());
}

TEST_CASE("message kind identifies request response and notification", "[mcp][types]")
{
  mcp::JsonRpcRequest request;
  request.id = mcp::RequestId::number(1);
  request.method = QStringLiteral("tools/list");

  mcp::JsonRpcResponse response;
  response.id = mcp::RequestId::number(1);
  response.result = QJsonValue{QStringLiteral("ok")};

  mcp::JsonRpcNotification notification;
  notification.method = QStringLiteral("notifications/initialized");

  const mcp::JsonRpcMessage request_message{request};
  const mcp::JsonRpcMessage response_message{response};
  const mcp::JsonRpcMessage notification_message{notification};

  REQUIRE(mcp::message_kind(request_message) == mcp::JsonRpcMessageKind::Request);
  REQUIRE(mcp::message_kind(response_message) == mcp::JsonRpcMessageKind::Response);
  REQUIRE(mcp::message_kind(notification_message) == mcp::JsonRpcMessageKind::Notification);
  REQUIRE(mcp::message_kind_name(mcp::JsonRpcMessageKind::Notification) == QStringLiteral("notification"));
}

TEST_CASE("response helpers separate successful and error responses", "[mcp][types]")
{
  mcp::JsonRpcResponse success;
  success.id = mcp::RequestId::string(QStringLiteral("initialize-1"));
  success.result = QJsonValue{QStringLiteral("initialized")};

  REQUIRE(success.is_success());
  REQUIRE_FALSE(success.is_error());

  mcp::JsonRpcResponse failure;
  failure.id = mcp::RequestId::string(QStringLiteral("initialize-1"));
  failure.error = mcp::JsonRpcError{-32601, QStringLiteral("Method not found"), std::nullopt};

  REQUIRE_FALSE(failure.is_success());
  REQUIRE(failure.is_error());
  REQUIRE(failure.error->code == -32601);
  REQUIRE(failure.error->message == QStringLiteral("Method not found"));
}
