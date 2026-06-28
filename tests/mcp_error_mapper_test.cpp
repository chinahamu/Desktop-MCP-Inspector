#include <catch2/catch_test_macros.hpp>

#include "McpError.hpp"
#include "McpTypes.hpp"

#include <QJsonObject>
#include <QJsonValue>
#include <QString>

TEST_CASE("MCP error mapper classifies standard JSON-RPC error codes", "[mcp][error]")
{
  REQUIRE(mcp::category_for_error_code(-32700) == mcp::McpErrorCategory::Parse);
  REQUIRE(mcp::category_for_error_code(-32600) == mcp::McpErrorCategory::InvalidRequest);
  REQUIRE(mcp::category_for_error_code(-32601) == mcp::McpErrorCategory::MethodNotFound);
  REQUIRE(mcp::category_for_error_code(-32602) == mcp::McpErrorCategory::InvalidParams);
  REQUIRE(mcp::category_for_error_code(-32603) == mcp::McpErrorCategory::Internal);
  REQUIRE(mcp::category_for_error_code(-32000) == mcp::McpErrorCategory::Server);
  REQUIRE(mcp::category_for_error_code(1234) == mcp::McpErrorCategory::Unknown);
}

TEST_CASE("MCP error mapper exposes category and severity names", "[mcp][error]")
{
  REQUIRE(mcp::category_name(mcp::McpErrorCategory::InvalidParams) == QStringLiteral("invalid_params"));
  REQUIRE(mcp::severity_name(mcp::McpErrorSeverity::Critical) == QStringLiteral("critical"));
  REQUIRE(mcp::severity_for_category(mcp::McpErrorCategory::Parse) == mcp::McpErrorSeverity::Critical);
  REQUIRE(mcp::severity_for_category(mcp::McpErrorCategory::MethodNotFound) == mcp::McpErrorSeverity::Error);
  REQUIRE(mcp::severity_for_category(mcp::McpErrorCategory::Unknown) == mcp::McpErrorSeverity::Warning);
}

TEST_CASE("MCP error descriptor contains UI-ready fields", "[mcp][error]")
{
  const mcp::JsonRpcError error{
      -32602,
      QStringLiteral("Missing required field"),
      QJsonValue{QJsonObject{{QStringLiteral("field"), QStringLiteral("name")}}},
  };

  const auto descriptor = mcp::describe_error(error);

  REQUIRE(descriptor.category == mcp::McpErrorCategory::InvalidParams);
  REQUIRE(descriptor.severity == mcp::McpErrorSeverity::Error);
  REQUIRE(descriptor.title == QStringLiteral("Invalid params"));
  REQUIRE(descriptor.detail.contains(QStringLiteral("Code: -32602")));
  REQUIRE(descriptor.detail.contains(QStringLiteral("Missing required field")));
  REQUIRE(descriptor.detail.contains(QStringLiteral("field")));
  REQUIRE(descriptor.recommendation.contains(QStringLiteral("server schema")));
  REQUIRE_FALSE(descriptor.retryable);
}

TEST_CASE("MCP error mapper marks server and internal errors retryable", "[mcp][error]")
{
  REQUIRE(mcp::is_retryable_error_code(-32001));
  REQUIRE(mcp::is_retryable_error_code(-32603));
  REQUIRE_FALSE(mcp::is_retryable_error_code(-32602));
}

TEST_CASE("MCP response without error maps to informational descriptor", "[mcp][error]")
{
  mcp::JsonRpcResponse response;
  response.id = mcp::RequestId::number(1);
  response.result = QJsonValue{QStringLiteral("ok")};

  const auto descriptor = mcp::describe_error(response);

  REQUIRE(descriptor.category == mcp::McpErrorCategory::Unknown);
  REQUIRE(descriptor.severity == mcp::McpErrorSeverity::Info);
  REQUIRE(descriptor.title == QStringLiteral("No MCP error"));
  REQUIRE_FALSE(descriptor.retryable);
}
