#include <catch2/catch_test_macros.hpp>

#include "McpMessage.hpp"
#include "McpTypes.hpp"

#include <QByteArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include <variant>

TEST_CASE("JSON-RPC request payload parses into request message", "[mcp][message]")
{
  const auto parsed = mcp::parse_json_rpc_message(QByteArray{R"({"jsonrpc":"2.0","id":"req-1","method":"tools/list","params":{"cursor":"next"}})"});

  const auto* message = std::get_if<mcp::JsonRpcMessage>(&parsed);
  REQUIRE(message != nullptr);

  const auto* request = std::get_if<mcp::JsonRpcRequest>(message);
  REQUIRE(request != nullptr);
  REQUIRE(request->id == mcp::RequestId::string(QStringLiteral("req-1")));
  REQUIRE(request->method == QStringLiteral("tools/list"));
  REQUIRE(request->params.has_value());
  REQUIRE(request->params->toObject().value(QStringLiteral("cursor")).toString() == QStringLiteral("next"));
}

TEST_CASE("JSON-RPC notification payload parses without id", "[mcp][message]")
{
  const auto parsed = mcp::parse_json_rpc_message(QByteArray{R"({"jsonrpc":"2.0","method":"notifications/initialized"})"});

  const auto* message = std::get_if<mcp::JsonRpcMessage>(&parsed);
  REQUIRE(message != nullptr);

  const auto* notification = std::get_if<mcp::JsonRpcNotification>(message);
  REQUIRE(notification != nullptr);
  REQUIRE(notification->method == QStringLiteral("notifications/initialized"));
  REQUIRE_FALSE(notification->params.has_value());
}

TEST_CASE("JSON-RPC success response payload parses result", "[mcp][message]")
{
  const auto parsed = mcp::parse_json_rpc_message(QByteArray{R"({"jsonrpc":"2.0","id":2,"result":{"protocolVersion":"2025-06-18"}})"});

  const auto* message = std::get_if<mcp::JsonRpcMessage>(&parsed);
  REQUIRE(message != nullptr);

  const auto* response = std::get_if<mcp::JsonRpcResponse>(message);
  REQUIRE(response != nullptr);
  REQUIRE(response->id == mcp::RequestId::number(2));
  REQUIRE(response->is_success());
  REQUIRE(response->result->toObject().value(QStringLiteral("protocolVersion")).toString() == QStringLiteral("2025-06-18"));
}

TEST_CASE("JSON-RPC error response payload parses error object", "[mcp][message]")
{
  const auto parsed = mcp::parse_json_rpc_message(QByteArray{R"({"jsonrpc":"2.0","id":"req-2","error":{"code":-32601,"message":"Method not found","data":{"method":"missing"}}})"});

  const auto* message = std::get_if<mcp::JsonRpcMessage>(&parsed);
  REQUIRE(message != nullptr);

  const auto* response = std::get_if<mcp::JsonRpcResponse>(message);
  REQUIRE(response != nullptr);
  REQUIRE(response->is_error());
  REQUIRE(response->error->code == -32601);
  REQUIRE(response->error->message == QStringLiteral("Method not found"));
  REQUIRE(response->error->data->toObject().value(QStringLiteral("method")).toString() == QStringLiteral("missing"));
}

TEST_CASE("JSON-RPC message serializes to compact JSON", "[mcp][message]")
{
  mcp::JsonRpcRequest request;
  request.id = mcp::RequestId::number(42);
  request.method = QStringLiteral("tools/call");
  request.params = QJsonObject{{QStringLiteral("name"), QStringLiteral("echo")}};

  const auto serialized = mcp::serialize_json_rpc_message(mcp::JsonRpcMessage{request});
  const auto reparsed = mcp::parse_json_rpc_message(serialized);

  const auto* message = std::get_if<mcp::JsonRpcMessage>(&reparsed);
  REQUIRE(message != nullptr);

  const auto* reparsed_request = std::get_if<mcp::JsonRpcRequest>(message);
  REQUIRE(reparsed_request != nullptr);
  REQUIRE(reparsed_request->id == mcp::RequestId::number(42));
  REQUIRE(reparsed_request->method == QStringLiteral("tools/call"));
  REQUIRE(reparsed_request->params->toObject().value(QStringLiteral("name")).toString() == QStringLiteral("echo"));
}

TEST_CASE("JSON-RPC parser reports invalid version and invalid response shape", "[mcp][message]")
{
  const auto invalid_version = mcp::parse_json_rpc_message(QByteArray{R"({"jsonrpc":"1.0","method":"tools/list"})"});
  const auto* version_error = std::get_if<mcp::JsonRpcParseError>(&invalid_version);
  REQUIRE(version_error != nullptr);
  REQUIRE(version_error->code == mcp::JsonRpcParseErrorCode::InvalidVersion);

  const auto invalid_response = mcp::parse_json_rpc_message(QByteArray{R"({"jsonrpc":"2.0","id":1,"result":{},"error":{"code":-32603,"message":"Internal error"}})"});
  const auto* response_error = std::get_if<mcp::JsonRpcParseError>(&invalid_response);
  REQUIRE(response_error != nullptr);
  REQUIRE(response_error->code == mcp::JsonRpcParseErrorCode::InvalidResponse);
}
