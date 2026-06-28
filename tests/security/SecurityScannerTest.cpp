#include "SecurityScanner.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>

TEST_CASE("SecurityScanner aggregates default security rules")
{
  security::SecurityScanInput input;

  config::ServerProfile profile;
  profile.name = QStringLiteral("Dangerous local shell profile");
  profile.transport = config::ServerTransportKind::Stdio;
  profile.command = QStringLiteral("powershell");
  profile.args = QStringList{
      QStringLiteral("-EncodedCommand"),
      QStringLiteral("AAAA"),
      QStringLiteral("Remove-Item"),
      QStringLiteral("C:/tmp/demo"),
      QStringLiteral("-Recurse"),
  };
  profile.env.insert(QStringLiteral("OPENAI_API_KEY"), QStringLiteral("sk-demo-token"));
  input.profile = profile;

  input.tools.push_back(mcp::McpTool{
      QStringLiteral("shell.execute"),
      QStringLiteral("Run shell commands, delete files, and access the filesystem."),
      QJsonObject{
          {QStringLiteral("type"), QStringLiteral("object")},
          {QStringLiteral("properties"), QJsonObject{
              {QStringLiteral("command"), QJsonObject{
                  {QStringLiteral("type"), QStringLiteral("string")},
                  {QStringLiteral("description"), QStringLiteral("Command or shell script to execute")},
              }},
              {QStringLiteral("path"), QJsonObject{
                  {QStringLiteral("type"), QStringLiteral("string")},
                  {QStringLiteral("description"), QStringLiteral("File path to modify")},
              }},
          }},
          {QStringLiteral("required"), QJsonArray{QStringLiteral("command")}},
          {QStringLiteral("additionalProperties"), true},
      },
  });

  input.resources.push_back(mcp::McpResource{
      QStringLiteral("file:///Users/demo/.env"),
      QStringLiteral("local-env-secret"),
      QStringLiteral("Local .env secret"),
      QStringLiteral("Credential material exposed as a resource."),
      QStringLiteral("text/plain"),
      128,
      QJsonObject{},
  });

  const auto scanner = security::SecurityScanner::default_scanner();
  const auto result = scanner.scan(input);

  REQUIRE(scanner.rule_count() == 6);
  REQUIRE(result.rule_count == 6);
  REQUIRE(result.findings.size() >= 6);
  CHECK(result.score.value == 0);
  CHECK(result.score.severity == security::RiskSeverity::Critical);
  CHECK_FALSE(result.passed);
}

TEST_CASE("SecurityScanner passes low-risk profiles")
{
  security::SecurityScanInput input;

  config::ServerProfile profile;
  profile.name = QStringLiteral("Reviewed server");
  profile.transport = config::ServerTransportKind::Stdio;
  profile.command = QStringLiteral("reviewed-mcp-server");
  profile.args = QStringList{QStringLiteral("--stdio")};
  input.profile = profile;

  input.tools.push_back(mcp::McpTool{
      QStringLiteral("echo"),
      QStringLiteral("Echo a text value."),
      QJsonObject{
          {QStringLiteral("type"), QStringLiteral("object")},
          {QStringLiteral("properties"), QJsonObject{
              {QStringLiteral("text"), QJsonObject{
                  {QStringLiteral("type"), QStringLiteral("string")},
                  {QStringLiteral("description"), QStringLiteral("Text to echo")},
              }},
          }},
      },
  });

  input.resources.push_back(mcp::McpResource{
      QStringLiteral("memory://demo/readme"),
      QStringLiteral("readme"),
      QStringLiteral("Readme"),
      QStringLiteral("Demo resource."),
      QStringLiteral("text/markdown"),
      256,
      QJsonObject{},
  });

  const auto result = security::SecurityScanner{}.scan(input);

  CHECK(result.findings.empty());
  CHECK(result.score.value == 100);
  CHECK(result.score.severity == security::RiskSeverity::Info);
  CHECK(result.passed);
}
