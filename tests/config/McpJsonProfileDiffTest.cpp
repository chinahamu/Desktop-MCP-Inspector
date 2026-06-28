#include "McpJson.hpp"
#include "ProfileDiff.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

TEST_CASE("mcp.json parser imports stdio and streamable HTTP profiles")
{
  const auto json = QString::fromUtf8(R"JSON(
{
  "mcpServers": {
    "filesystem": {
      "command": "npx",
      "args": ["-y", "@modelcontextprotocol/server-filesystem", "/tmp"],
      "env": {"API_TOKEN": "secret"},
      "cwd": "/workspace",
      "timeoutMs": 5000,
      "protocolVersion": "2025-06-18"
    },
    "remote": {
      "type": "streamable-http",
      "url": "https://example.test/mcp",
      "headers": {"Authorization": "Bearer token"}
    }
  }
}
)JSON");

  const auto result = config::parse_mcp_json(json);

  REQUIRE(result.ok());
  REQUIRE(result.profiles.size() == 2);

  const auto stdio = result.profiles.at(0);
  CHECK(stdio.id == QStringLiteral("filesystem"));
  CHECK(stdio.name == QStringLiteral("filesystem"));
  CHECK(stdio.transport == config::ServerTransportKind::Stdio);
  CHECK(stdio.command == QStringLiteral("npx"));
  CHECK((stdio.args == QStringList{QStringLiteral("-y"), QStringLiteral("@modelcontextprotocol/server-filesystem"), QStringLiteral("/tmp")}));
  CHECK(stdio.env.value(QStringLiteral("API_TOKEN")) == QStringLiteral("secret"));
  REQUIRE(stdio.timeout_ms.has_value());
  CHECK(*stdio.timeout_ms == 5000);

  const auto remote = result.profiles.at(1);
  CHECK(remote.transport == config::ServerTransportKind::StreamableHttp);
  CHECK(remote.endpoint_url == QStringLiteral("https://example.test/mcp"));
  CHECK(remote.http_headers.value(QStringLiteral("Authorization")) == QStringLiteral("Bearer token"));
  CHECK(remote.protocol_version == config::default_mcp_protocol_version());
}

TEST_CASE("mcp.json exporter writes mcpServers document")
{
  config::ServerProfile profile;
  profile.name = QStringLiteral("demo");
  profile.command = QStringLiteral("python");
  profile.args = QStringList{QStringLiteral("-m"), QStringLiteral("demo_server")};
  profile.env.insert(QStringLiteral("DEBUG"), QStringLiteral("1"));
  profile.timeout_ms = 3000;

  const auto exported = config::export_mcp_json(QVector<config::ServerProfile>{profile});
  const auto document = QJsonDocument::fromJson(exported.toUtf8());

  REQUIRE(document.isObject());
  const auto root = document.object();
  REQUIRE(root.value(QStringLiteral("mcpServers")).isObject());
  const auto servers = root.value(QStringLiteral("mcpServers")).toObject();
  REQUIRE(servers.value(QStringLiteral("demo")).isObject());
  const auto server = servers.value(QStringLiteral("demo")).toObject();
  CHECK(server.value(QStringLiteral("command")).toString() == QStringLiteral("python"));
  CHECK(server.value(QStringLiteral("args")).toArray().size() == 2);
  CHECK(server.value(QStringLiteral("timeoutMs")).toInt() == 3000);
}

TEST_CASE("mcp.json validation reports malformed documents")
{
  const auto result = config::parse_mcp_json(QStringLiteral(R"JSON({"servers": {}})JSON"));

  CHECK_FALSE(result.ok());
  CHECK(result.validation.has_errors());
  CHECK(result.summary().contains(QStringLiteral("mcpServers")));
}

TEST_CASE("profile diff reports changed fields")
{
  config::ServerProfile before;
  before.id = QStringLiteral("demo");
  before.name = QStringLiteral("Demo");
  before.command = QStringLiteral("node");
  before.args = QStringList{QStringLiteral("server.js")};
  before.protocol_version = QStringLiteral("2024-11-05");

  auto after = before;
  after.command = QStringLiteral("python");
  after.args = QStringList{QStringLiteral("-m"), QStringLiteral("server")};
  after.protocol_version = QStringLiteral("2025-06-18");

  const auto diff = config::diff_profiles(std::optional<config::ServerProfile>{before}, after);

  REQUIRE(diff.has_changes());
  CHECK(diff.entries.size() == 3);
  CHECK(diff.summary().contains(QStringLiteral("command")));
  CHECK(diff.summary().contains(QStringLiteral("protocol_version")));
}

TEST_CASE("profile set diff detects added and removed profiles")
{
  config::ServerProfile before;
  before.name = QStringLiteral("old-server");
  before.command = QStringLiteral("node");

  config::ServerProfile after;
  after.name = QStringLiteral("new-server");
  after.command = QStringLiteral("python");

  const auto diff = config::diff_profile_sets(
      QVector<config::ServerProfile>{before},
      QVector<config::ServerProfile>{after});

  CHECK(diff.has_changes());
  CHECK(diff.added_profiles.size() == 1);
  CHECK(diff.removed_profiles.size() == 1);
  CHECK(diff.summary().contains(QStringLiteral("Profile added")));
}
