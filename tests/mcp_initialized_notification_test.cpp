#include <catch2/catch_test_macros.hpp>

#include "McpInitialize.hpp"
#include "McpMessage.hpp"
#include "McpTypes.hpp"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include <variant>

TEST_CASE("initialized notification uses MCP initialized method without params", "[mcp][initialize]")
{
  const auto notification = mcp::make_initialized_notification();

  REQUIRE(notification.method == QStringLiteral("notifications/initialized"));
  REQUIRE_FALSE(notification.params.has_value());
  REQUIRE(mcp::is_initialized_notification(notification));
}

TEST_CASE("initialized notification serializes as JSON-RPC notification", "[mcp][initialize]")
{
  const mcp::JsonRpcMessage message = mcp::make_initialized_notification();

  const auto payload = mcp::serialize_json_rpc_message(message);
  const auto document = QJsonDocument::fromJson(payload);
  const auto object = document.object();

  REQUIRE(object.value(QStringLiteral("jsonrpc")).toString() == QStringLiteral("2.0"));
  REQUIRE(object.value(QStringLiteral("method")).toString() == QStringLiteral("notifications/initialized"));
  REQUIRE_FALSE(object.contains(QStringLiteral("id")));
  REQUIRE_FALSE(object.contains(QStringLiteral("params")));
}

TEST_CASE("initialized notification parses back as notification", "[mcp][initialize]")
{
  const QByteArray payload{R"({"jsonrpc":"2.0","method":"notifications/initialized"})"};

  const auto parsed = mcp::parse_json_rpc_message(payload);
  const auto* message = std::get_if<mcp::JsonRpcMessage>(&parsed);

  REQUIRE(message != nullptr);
  REQUIRE(mcp::message_kind(*message) == mcp::JsonRpcMessageKind::Notification);

  const auto* notification = std::get_if<mcp::JsonRpcNotification>(message);
  REQUIRE(notification != nullptr);
  REQUIRE(mcp::is_initialized_notification(*notification));
}

TEST_CASE("initialized notification matcher rejects params and other methods", "[mcp][initialize]")
{
  mcp::JsonRpcNotification with_params;
  with_params.method = QStringLiteral("notifications/initialized");
  with_params.params = QJsonValue{QJsonObject{}};
  REQUIRE_FALSE(mcp::is_initialized_notification(with_params));

  mcp::JsonRpcNotification other_method;
  other_method.method = QStringLiteral("notifications/cancelled");
  REQUIRE_FALSE(mcp::is_initialized_notification(other_method));
}
