#include <catch2/catch_test_macros.hpp>

#include "ServerProfile.hpp"
#include "StdioLaunchSpec.hpp"

#include <QString>
#include <QStringList>

#include <optional>

TEST_CASE("stdio launch spec keeps program separate from arguments", "[transport][stdio]")
{
  config::ServerProfile profile;
  profile.command = QStringLiteral("/Applications/My MCP Server/bin/server");
  profile.args = QStringList{
      QStringLiteral("--config"),
      QStringLiteral("/Users/example/MCP Profiles/dev profile.json"),
  };

  const auto spec = transport::make_stdio_launch_spec(profile);

  REQUIRE(spec.program == profile.command);
  REQUIRE(spec.arguments == profile.args);
  REQUIRE(spec.program != QStringLiteral("/Applications/My MCP Server/bin/server --config /Users/example/MCP Profiles/dev profile.json"));
}

TEST_CASE("stdio launch spec preserves quote-sensitive argument tokens", "[transport][stdio]")
{
  config::ServerProfile profile;
  profile.command = QStringLiteral("node");
  profile.args = QStringList{
      QStringLiteral("server.js"),
      QStringLiteral("--name=Desktop MCP Inspector"),
      QStringLiteral("--json={\"enabled\":true}"),
      QStringLiteral("value with spaces"),
      QStringLiteral("quoted \"inner\" value"),
      QStringLiteral(""),
      QStringLiteral("日本語 argument"),
  };

  const auto spec = transport::make_stdio_launch_spec(profile);

  REQUIRE(spec.arguments.size() == 7);
  REQUIRE(spec.arguments.at(0) == QStringLiteral("server.js"));
  REQUIRE(spec.arguments.at(1) == QStringLiteral("--name=Desktop MCP Inspector"));
  REQUIRE(spec.arguments.at(2) == QStringLiteral("--json={\"enabled\":true}"));
  REQUIRE(spec.arguments.at(3) == QStringLiteral("value with spaces"));
  REQUIRE(spec.arguments.at(4) == QStringLiteral("quoted \"inner\" value"));
  REQUIRE(spec.arguments.at(5).isEmpty());
  REQUIRE(spec.arguments.at(6) == QStringLiteral("日本語 argument"));
}

TEST_CASE("stdio launch spec preserves platform-specific path-like arguments", "[transport][stdio]")
{
  config::ServerProfile profile;
  profile.command = QStringLiteral("python");
  profile.args = QStringList{
      QStringLiteral("C:\\Program Files\\MCP Server\\server.py"),
      QStringLiteral("/Users/example/Library/Application Support/MCP/server.py"),
      QStringLiteral("/home/example/.config/mcp/server.py"),
  };

  const auto spec = transport::make_stdio_launch_spec(profile);

  REQUIRE(spec.arguments == profile.args);
}

TEST_CASE("stdio launch spec carries cwd env and timeout unchanged", "[transport][stdio]")
{
  config::ServerProfile profile;
  profile.command = QStringLiteral("server");
  profile.args = QStringList{QStringLiteral("--verbose")};
  profile.cwd = QStringLiteral("/tmp/path with spaces");
  profile.env.insert(QStringLiteral("MCP_TOKEN"), QStringLiteral("secret with spaces"));
  profile.timeout_ms = 12345;

  const auto spec = transport::make_stdio_launch_spec(profile);

  REQUIRE(spec.working_directory == profile.cwd);
  REQUIRE(spec.environment.value(QStringLiteral("MCP_TOKEN")) == QStringLiteral("secret with spaces"));
  REQUIRE(spec.timeout_ms == std::optional<qint64>{12345});
}
