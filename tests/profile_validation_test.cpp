#include <catch2/catch_test_macros.hpp>

#include "ProfileValidation.hpp"
#include "ServerProfile.hpp"

#include <QString>

TEST_CASE("profile validation rejects empty commands", "[config][profile]")
{
  config::ServerProfile profile;
  profile.name = QStringLiteral("Broken");
  profile.command = QStringLiteral("  ");

  const auto result = config::validate_profile(profile);

  REQUIRE(result.has_errors());
  REQUIRE_FALSE(result.ok());
  REQUIRE(result.error_messages().join(QStringLiteral("\n")).contains(QStringLiteral("Command")));
}

TEST_CASE("profile validation rejects invalid timeout and env keys", "[config][profile]")
{
  config::ServerProfile profile;
  profile.name = QStringLiteral("Invalid env");
  profile.command = QStringLiteral("node");
  profile.timeout_ms = 0;
  profile.env.insert(QStringLiteral("1INVALID"), QStringLiteral("value"));

  const auto result = config::validate_profile(profile);

  REQUIRE(result.has_errors());
  REQUIRE(result.error_messages().size() == 2);
}

TEST_CASE("profile validation accepts a complete stdio profile", "[config][profile]")
{
  config::ServerProfile profile;
  profile.name = QStringLiteral("Echo server");
  profile.command = QStringLiteral("node");
  profile.args = {QStringLiteral("server.js")};
  profile.cwd = QStringLiteral("/tmp");
  profile.timeout_ms = 3000;
  profile.env.insert(QStringLiteral("API_TOKEN"), QStringLiteral("secret"));

  const auto result = config::validate_profile(profile);

  REQUIRE(result.ok());
}

TEST_CASE("profile validation accepts a complete streamable HTTP profile", "[config][profile][http]")
{
  config::ServerProfile profile;
  profile.name = QStringLiteral("HTTP MCP server");
  profile.transport = config::ServerTransportKind::StreamableHttp;
  profile.endpoint_url = QStringLiteral("https://example.test/mcp");
  profile.protocol_version = QStringLiteral("2025-06-18");
  profile.http_headers.insert(QStringLiteral("Authorization"), QStringLiteral("Bearer token"));

  const auto result = config::validate_profile(profile);

  REQUIRE(result.ok());
}

TEST_CASE("profile validation rejects invalid streamable HTTP endpoints and headers", "[config][profile][http]")
{
  config::ServerProfile profile;
  profile.name = QStringLiteral("Bad HTTP MCP server");
  profile.transport = config::ServerTransportKind::StreamableHttp;
  profile.endpoint_url = QStringLiteral("file:///tmp/mcp.sock");
  profile.http_headers.insert(QStringLiteral("Bad Header"), QStringLiteral("value"));

  const auto result = config::validate_profile(profile);

  REQUIRE(result.has_errors());
  const auto errors = result.error_messages().join(QStringLiteral("\n"));
  REQUIRE(errors.contains(QStringLiteral("HTTP endpoint")));
  REQUIRE(errors.contains(QStringLiteral("HTTP header")));
}
