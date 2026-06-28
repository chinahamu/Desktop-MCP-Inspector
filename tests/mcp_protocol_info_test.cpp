#include <catch2/catch_test_macros.hpp>

#include "McpTypes.hpp"

#include <QJsonObject>
#include <QString>

TEST_CASE("default MCP protocol version is valid", "[mcp][types]")
{
  const auto protocol_version = mcp::default_protocol_version();

  REQUIRE(protocol_version.value == QStringLiteral("2025-06-18"));
  REQUIRE(mcp::is_valid_protocol_version(protocol_version));
}

TEST_CASE("default MCP client info uses application name and provided version", "[mcp][types]")
{
  const auto client_info = mcp::default_client_info(QStringLiteral("0.1.0"));

  REQUIRE(client_info.name == QStringLiteral("Desktop MCP Inspector"));
  REQUIRE(client_info.version == QStringLiteral("0.1.0"));
  REQUIRE(mcp::is_valid_implementation_info(client_info));
}

TEST_CASE("MCP implementation info serializes to JSON object", "[mcp][types]")
{
  const mcp::McpImplementationInfo implementation_info{
      QStringLiteral("Desktop MCP Inspector"),
      QStringLiteral("0.1.0"),
  };

  const auto object = mcp::to_json_object(implementation_info);

  REQUIRE(object.value(QStringLiteral("name")).toString() == QStringLiteral("Desktop MCP Inspector"));
  REQUIRE(object.value(QStringLiteral("version")).toString() == QStringLiteral("0.1.0"));
}

TEST_CASE("MCP implementation info parses from JSON object", "[mcp][types]")
{
  const QJsonObject object{
      {QStringLiteral("name"), QStringLiteral("example-server")},
      {QStringLiteral("version"), QStringLiteral("1.2.3")},
  };

  const auto parsed = mcp::implementation_info_from_json_object(object);

  REQUIRE(parsed.has_value());
  REQUIRE(parsed->name == QStringLiteral("example-server"));
  REQUIRE(parsed->version == QStringLiteral("1.2.3"));
}

TEST_CASE("MCP implementation info rejects missing or empty fields", "[mcp][types]")
{
  REQUIRE_FALSE(mcp::implementation_info_from_json_object(QJsonObject{}).has_value());

  const QJsonObject empty_name{
      {QStringLiteral("name"), QStringLiteral("   ")},
      {QStringLiteral("version"), QStringLiteral("1.0.0")},
  };
  REQUIRE_FALSE(mcp::implementation_info_from_json_object(empty_name).has_value());

  const QJsonObject empty_version{
      {QStringLiteral("name"), QStringLiteral("Desktop MCP Inspector")},
      {QStringLiteral("version"), QStringLiteral(" ")},
  };
  REQUIRE_FALSE(mcp::implementation_info_from_json_object(empty_version).has_value());
}

TEST_CASE("MCP protocol version validation rejects blank values", "[mcp][types]")
{
  REQUIRE_FALSE(mcp::is_valid_protocol_version(mcp::McpProtocolVersion{QStringLiteral(" ")}));
}
