#include <catch2/catch_test_macros.hpp>

#include "McpInitialize.hpp"
#include "McpTypes.hpp"

#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include <optional>
#include <variant>

TEST_CASE("initialize params serialize to MCP JSON object", "[mcp][initialize]")
{
  const mcp::McpInitializeParams params{
      mcp::McpProtocolVersion{QStringLiteral("2025-06-18")},
      mcp::McpClientInfo{QStringLiteral("Desktop MCP Inspector"), QStringLiteral("0.1.0")},
      QJsonObject{{QStringLiteral("roots"), QJsonObject{{QStringLiteral("listChanged"), true}}}},
  };

  const auto object = mcp::to_json_object(params);

  REQUIRE(object.value(QStringLiteral("protocolVersion")).toString() == QStringLiteral("2025-06-18"));
  REQUIRE(object.value(QStringLiteral("clientInfo")).toObject().value(QStringLiteral("name")).toString() == QStringLiteral("Desktop MCP Inspector"));
  REQUIRE(object.value(QStringLiteral("capabilities")).toObject().contains(QStringLiteral("roots")));
}

TEST_CASE("initialize request is tracked in pending request store", "[mcp][initialize]")
{
  mcp::PendingRequestStore pending_requests;

  const auto request = mcp::make_initialize_request(
      pending_requests,
      mcp::default_client_info(QStringLiteral("0.1.0")),
      QJsonObject{{QStringLiteral("sampling"), QJsonObject{}}});

  REQUIRE(request.id == mcp::RequestId::number(1));
  REQUIRE(request.method == QStringLiteral("initialize"));
  REQUIRE(request.params.has_value());

  const auto params = request.params->toObject();
  REQUIRE(params.value(QStringLiteral("protocolVersion")).toString() == QStringLiteral("2025-06-18"));
  REQUIRE(params.value(QStringLiteral("clientInfo")).toObject().value(QStringLiteral("version")).toString() == QStringLiteral("0.1.0"));
  REQUIRE(params.value(QStringLiteral("capabilities")).toObject().contains(QStringLiteral("sampling")));
  REQUIRE(pending_requests.contains(request.id));
}

TEST_CASE("initialize response parses protocol version capabilities and server info", "[mcp][initialize]")
{
  mcp::JsonRpcResponse response;
  response.id = mcp::RequestId::number(1);
  response.result = QJsonValue{QJsonObject{
      {QStringLiteral("protocolVersion"), QStringLiteral("2025-06-18")},
      {QStringLiteral("capabilities"), QJsonObject{{QStringLiteral("tools"), QJsonObject{}}}},
      {QStringLiteral("serverInfo"), QJsonObject{{QStringLiteral("name"), QStringLiteral("test-server")}, {QStringLiteral("version"), QStringLiteral("1.0.0")}}},
      {QStringLiteral("instructions"), QStringLiteral("Use carefully")},
  }};

  const auto parsed = mcp::parse_initialize_response(response);
  const auto* result = std::get_if<mcp::McpInitializeResult>(&parsed);

  REQUIRE(result != nullptr);
  REQUIRE(result->protocol_version.value == QStringLiteral("2025-06-18"));
  REQUIRE(result->capabilities.contains(QStringLiteral("tools")));
  REQUIRE(result->server_info.name == QStringLiteral("test-server"));
  REQUIRE(result->server_info.version == QStringLiteral("1.0.0"));
  REQUIRE(result->instructions == QStringLiteral("Use carefully"));
}

TEST_CASE("initialize response maps JSON-RPC error into parse error", "[mcp][initialize]")
{
  mcp::JsonRpcResponse response;
  response.id = mcp::RequestId::number(1);
  response.error = mcp::JsonRpcError{-32602, QStringLiteral("Invalid initialize params"), std::nullopt};

  const auto parsed = mcp::parse_initialize_response(response);
  const auto* error = std::get_if<mcp::McpInitializeParseError>(&parsed);

  REQUIRE(error != nullptr);
  REQUIRE(error->code == mcp::McpInitializeParseErrorCode::ResponseError);
  REQUIRE(error->message == QStringLiteral("Invalid initialize params"));
}

TEST_CASE("initialize result validates required fields", "[mcp][initialize]")
{
  const auto missing_protocol = mcp::parse_initialize_result(QJsonObject{
      {QStringLiteral("capabilities"), QJsonObject{}},
      {QStringLiteral("serverInfo"), QJsonObject{{QStringLiteral("name"), QStringLiteral("server")}, {QStringLiteral("version"), QStringLiteral("1")}}},
  });
  REQUIRE(std::get<mcp::McpInitializeParseError>(missing_protocol).code == mcp::McpInitializeParseErrorCode::InvalidProtocolVersion);

  const auto invalid_capabilities = mcp::parse_initialize_result(QJsonObject{
      {QStringLiteral("protocolVersion"), QStringLiteral("2025-06-18")},
      {QStringLiteral("capabilities"), QStringLiteral("invalid")},
      {QStringLiteral("serverInfo"), QJsonObject{{QStringLiteral("name"), QStringLiteral("server")}, {QStringLiteral("version"), QStringLiteral("1")}}},
  });
  REQUIRE(std::get<mcp::McpInitializeParseError>(invalid_capabilities).code == mcp::McpInitializeParseErrorCode::InvalidCapabilities);

  const auto invalid_server_info = mcp::parse_initialize_result(QJsonObject{
      {QStringLiteral("protocolVersion"), QStringLiteral("2025-06-18")},
      {QStringLiteral("capabilities"), QJsonObject{}},
      {QStringLiteral("serverInfo"), QJsonObject{{QStringLiteral("name"), QStringLiteral("server")}}},
  });
  REQUIRE(std::get<mcp::McpInitializeParseError>(invalid_server_info).code == mcp::McpInitializeParseErrorCode::InvalidServerInfo);
}
