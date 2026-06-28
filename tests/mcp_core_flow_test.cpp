#include <catch2/catch_test_macros.hpp>

#include "McpCapabilities.hpp"
#include "McpClient.hpp"
#include "McpError.hpp"
#include "McpInitialize.hpp"
#include "McpMessage.hpp"
#include "McpTypes.hpp"

#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include <optional>
#include <variant>

TEST_CASE("MCP core initialize flow tracks request and server capabilities", "[mcp][core]")
{
  mcp::PendingRequestStore pending_requests;
  mcp::McpCapabilitiesStore capabilities_store;

  const auto initialize_request = mcp::make_initialize_request(
      pending_requests,
      mcp::default_client_info(QStringLiteral("0.1.0")),
      QJsonObject{{QStringLiteral("roots"), QJsonObject{{QStringLiteral("listChanged"), true}}}});

  REQUIRE(initialize_request.id == mcp::RequestId::number(1));
  REQUIRE(initialize_request.method == QStringLiteral("initialize"));
  REQUIRE(pending_requests.contains(initialize_request.id));

  const auto pending_initialize = pending_requests.find(initialize_request.id);
  REQUIRE(pending_initialize.has_value());
  REQUIRE(pending_initialize->method == QStringLiteral("initialize"));

  const auto initialize_params = initialize_request.params->toObject();
  capabilities_store.set_client_capabilities(mcp::capabilities_from_json_object(
      initialize_params.value(QStringLiteral("capabilities")).toObject()));
  REQUIRE(capabilities_store.client_supports(mcp::McpCapabilityKey::Roots));
  REQUIRE(capabilities_store.client_capabilities()->list_changed_supported(mcp::McpCapabilityKey::Roots));

  mcp::JsonRpcResponse response;
  response.id = initialize_request.id;
  response.result = QJsonValue{QJsonObject{
      {QStringLiteral("protocolVersion"), QStringLiteral("2025-06-18")},
      {QStringLiteral("capabilities"), QJsonObject{{QStringLiteral("tools"), QJsonObject{{QStringLiteral("listChanged"), true}}}}},
      {QStringLiteral("serverInfo"), QJsonObject{{QStringLiteral("name"), QStringLiteral("test-server")}, {QStringLiteral("version"), QStringLiteral("1.0.0")}}},
  }};

  const auto parsed_initialize = mcp::parse_initialize_response(response);
  const auto* initialize_result = std::get_if<mcp::McpInitializeResult>(&parsed_initialize);
  REQUIRE(initialize_result != nullptr);

  capabilities_store.set_from_initialize_result(*initialize_result);
  REQUIRE(capabilities_store.server_supports(mcp::McpCapabilityKey::Tools));
  REQUIRE(capabilities_store.server_capabilities()->list_changed_supported(mcp::McpCapabilityKey::Tools));

  const auto completed = pending_requests.complete(response.id);
  REQUIRE(completed.has_value());
  REQUIRE(completed->id == initialize_request.id);
  REQUIRE(pending_requests.empty());
}

TEST_CASE("MCP core initialized notification round trips through JSON-RPC", "[mcp][core]")
{
  const mcp::JsonRpcMessage outbound = mcp::make_initialized_notification();
  const auto payload = mcp::serialize_json_rpc_message(outbound);

  const auto parsed = mcp::parse_json_rpc_message(payload);
  const auto* parsed_message = std::get_if<mcp::JsonRpcMessage>(&parsed);

  REQUIRE(parsed_message != nullptr);
  REQUIRE(mcp::message_kind(*parsed_message) == mcp::JsonRpcMessageKind::Notification);

  const auto* notification = std::get_if<mcp::JsonRpcNotification>(parsed_message);
  REQUIRE(notification != nullptr);
  REQUIRE(mcp::is_initialized_notification(*notification));
}

TEST_CASE("MCP core maps initialize errors to UI descriptors", "[mcp][core]")
{
  mcp::JsonRpcResponse response;
  response.id = mcp::RequestId::number(1);
  response.error = mcp::JsonRpcError{-32602, QStringLiteral("Invalid initialize params"), QJsonValue{QJsonObject{{QStringLiteral("field"), QStringLiteral("capabilities")}}}};

  const auto parsed = mcp::parse_initialize_response(response);
  const auto* parse_error = std::get_if<mcp::McpInitializeParseError>(&parsed);
  REQUIRE(parse_error != nullptr);
  REQUIRE(parse_error->code == mcp::McpInitializeParseErrorCode::ResponseError);

  const auto descriptor = mcp::describe_error(*response.error);
  REQUIRE(descriptor.category == mcp::McpErrorCategory::InvalidParams);
  REQUIRE(descriptor.severity == mcp::McpErrorSeverity::Error);
  REQUIRE(descriptor.title == QStringLiteral("Invalid params"));
  REQUIRE(descriptor.detail.contains(QStringLiteral("capabilities")));
  REQUIRE_FALSE(descriptor.retryable);
}

TEST_CASE("MCP core request lifecycle expires pending requests", "[mcp][core]")
{
  mcp::PendingRequestStore pending_requests;

  const auto request = pending_requests.make_request(
      QStringLiteral("tools/list"),
      std::nullopt,
      std::optional<qint64>{100});

  const auto pending = pending_requests.find(request.id);
  REQUIRE(pending.has_value());
  REQUIRE_FALSE(pending_requests.has_timed_out(request.id, pending->created_at.addMSecs(99)));
  REQUIRE(pending_requests.has_timed_out(request.id, pending->created_at.addMSecs(100)));

  const auto expired = pending_requests.expire_timed_out_requests(pending->created_at.addMSecs(100));
  REQUIRE(expired.size() == 1);
  REQUIRE(expired.front().id == request.id);
  REQUIRE_FALSE(pending_requests.contains(request.id));
}

TEST_CASE("MCP core parse serialize preserves request response and notification kinds", "[mcp][core]")
{
  const mcp::JsonRpcMessage request_message = mcp::JsonRpcRequest{
      mcp::RequestId::string(QStringLiteral("req-1")),
      QStringLiteral("tools/list"),
      QJsonValue{QJsonObject{{QStringLiteral("cursor"), QStringLiteral("next")}}},
  };
  const auto request_parsed = mcp::parse_json_rpc_message(mcp::serialize_json_rpc_message(request_message));
  const auto* request_roundtrip = std::get_if<mcp::JsonRpcMessage>(&request_parsed);
  REQUIRE(request_roundtrip != nullptr);
  REQUIRE(mcp::message_kind(*request_roundtrip) == mcp::JsonRpcMessageKind::Request);

  const mcp::JsonRpcMessage response_message = mcp::JsonRpcResponse{
      mcp::RequestId::number(2),
      QJsonValue{QJsonObject{{QStringLiteral("ok"), true}}},
      std::nullopt,
  };
  const auto response_parsed = mcp::parse_json_rpc_message(mcp::serialize_json_rpc_message(response_message));
  const auto* response_roundtrip = std::get_if<mcp::JsonRpcMessage>(&response_parsed);
  REQUIRE(response_roundtrip != nullptr);
  REQUIRE(mcp::message_kind(*response_roundtrip) == mcp::JsonRpcMessageKind::Response);

  const mcp::JsonRpcMessage notification_message = mcp::JsonRpcNotification{
      QStringLiteral("notifications/progress"),
      QJsonValue{QJsonObject{{QStringLiteral("progress"), QJsonValue{1}}}},
  };
  const auto notification_parsed = mcp::parse_json_rpc_message(mcp::serialize_json_rpc_message(notification_message));
  const auto* notification_roundtrip = std::get_if<mcp::JsonRpcMessage>(&notification_parsed);
  REQUIRE(notification_roundtrip != nullptr);
  REQUIRE(mcp::message_kind(*notification_roundtrip) == mcp::JsonRpcMessageKind::Notification);
}
