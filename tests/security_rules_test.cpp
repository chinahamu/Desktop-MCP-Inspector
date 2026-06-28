#include <catch2/catch_test_macros.hpp>

#include "RiskFinding.hpp"
#include "RuleEngine.hpp"
#include "SecurityRules.hpp"

#include <QJsonObject>
#include <QString>

#include <algorithm>
#include <stdexcept>
#include <vector>

namespace {

[[nodiscard]] bool has_finding_id(const std::vector<security::RiskFinding>& findings, const QString& id)
{
  return std::any_of(findings.begin(), findings.end(), [&id](const security::RiskFinding& finding) {
    return finding.id == id;
  });
}

[[nodiscard]] bool has_category(
    const std::vector<security::RiskFinding>& findings,
    security::RiskCategory category,
    security::RiskSeverity severity)
{
  return std::any_of(findings.begin(), findings.end(), [category, severity](const security::RiskFinding& finding) {
    return finding.category == category && finding.severity == severity;
  });
}

} // namespace

TEST_CASE("RiskFinding scoring summarizes highest severity and score", "[security]")
{
  const std::vector<security::RiskFinding> findings{
      security::RiskFinding{
          QStringLiteral("high"),
          security::RiskSeverity::High,
          security::RiskCategory::ProfileCommand,
          QStringLiteral("High"),
          {},
          {},
          {},
      },
      security::RiskFinding{
          QStringLiteral("low"),
          security::RiskSeverity::Low,
          security::RiskCategory::ToolMetadata,
          QStringLiteral("Low"),
          {},
          {},
          {},
      },
  };

  const auto score = security::calculate_risk_score(findings);

  REQUIRE(score.value == 70);
  REQUIRE(score.severity == security::RiskSeverity::High);
  REQUIRE(score.summary.contains(QStringLiteral("2")));
  REQUIRE(security::to_string(security::RiskCategory::InputSchema) == QStringLiteral("tool.inputSchema"));
}

TEST_CASE("RuleEngine rejects duplicate rule ids and aggregates findings", "[security][rules]")
{
  security::RuleEngine engine;
  const security::RuleDefinition rule{
      QStringLiteral("security.test.rule"),
      QStringLiteral("Test rule"),
      security::RiskCategory::ToolMetadata,
      [](const security::RuleInput&) {
        return std::vector<security::RiskFinding>{security::RiskFinding{
            QStringLiteral("finding"),
            security::RiskSeverity::Low,
            security::RiskCategory::ToolMetadata,
            QStringLiteral("Finding"),
            {},
            {},
            {},
        }};
      },
  };

  engine.add_rule(rule);

  REQUIRE(engine.count() == 1);
  REQUIRE_THROWS_AS(engine.add_rule(rule), std::invalid_argument);
  REQUIRE(engine.evaluate(security::RuleInput{}).size() == 1);
}

TEST_CASE("Command and environment rules detect risky startup configuration", "[security][rules]")
{
  config::ServerProfile profile;
  profile.transport = config::ServerTransportKind::Stdio;
  profile.command = QStringLiteral("powershell.exe");
  profile.args = QStringList{
      QStringLiteral("-EncodedCommand"),
      QStringLiteral("abc"),
      QStringLiteral("|"),
      QStringLiteral("Remove-Item"),
      QStringLiteral("C:/tmp"),
  };
  profile.env.insert(QStringLiteral("GITHUB_TOKEN"), QStringLiteral("ghp_secret_value"));

  const auto command_findings = security::evaluate_profile_command(profile);
  const auto env_findings = security::evaluate_profile_environment(profile);

  REQUIRE(has_finding_id(command_findings, QStringLiteral("security.command.shell")));
  REQUIRE(has_finding_id(command_findings, QStringLiteral("security.command.encoded")));
  REQUIRE(has_finding_id(command_findings, QStringLiteral("security.command.destructive")));
  REQUIRE(has_finding_id(command_findings, QStringLiteral("security.command.shell-metachar")));
  REQUIRE(has_category(env_findings, security::RiskCategory::ProfileEnvironment, security::RiskSeverity::High));
  REQUIRE(!env_findings.front().detail.contains(QStringLiteral("ghp_secret_value")));
}

TEST_CASE("HTTP endpoint rules detect cleartext and embedded credentials", "[security][rules]")
{
  config::ServerProfile profile;
  profile.transport = config::ServerTransportKind::StreamableHttp;
  profile.endpoint_url = QStringLiteral("http://user:pass@example.com/mcp?token=abc");

  const auto findings = security::evaluate_http_endpoint(profile);

  REQUIRE(has_finding_id(findings, QStringLiteral("security.http.cleartext")));
  REQUIRE(has_finding_id(findings, QStringLiteral("security.http.embedded-credentials")));
  REQUIRE(has_finding_id(findings, QStringLiteral("security.http.query-secret.token")));
}

TEST_CASE("Tool metadata and input schema rules flag dangerous capabilities", "[security][rules]")
{
  const mcp::McpTool tool{
      QStringLiteral("run_shell_delete_file"),
      QStringLiteral("Execute shell command and delete files"),
      QJsonObject{
          {QStringLiteral("type"), QStringLiteral("object")},
          {QStringLiteral("additionalProperties"), true},
          {QStringLiteral("properties"), QJsonObject{
              {QStringLiteral("command"), QJsonObject{
                  {QStringLiteral("type"), QStringLiteral("string")},
                  {QStringLiteral("description"), QStringLiteral("Shell command to run")},
              }},
              {QStringLiteral("apiKey"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}},
              {QStringLiteral("filePath"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}},
              {QStringLiteral("callbackUrl"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}},
          }},
      },
  };

  const auto metadata_findings = security::evaluate_tool_metadata(tool);
  const auto schema_findings = security::evaluate_input_schema(tool);

  REQUIRE(has_category(metadata_findings, security::RiskCategory::ToolMetadata, security::RiskSeverity::High));
  REQUIRE(has_category(metadata_findings, security::RiskCategory::ToolMetadata, security::RiskSeverity::Medium));
  REQUIRE(has_category(schema_findings, security::RiskCategory::InputSchema, security::RiskSeverity::High));
  REQUIRE(has_category(schema_findings, security::RiskCategory::InputSchema, security::RiskSeverity::Medium));
  REQUIRE(has_category(schema_findings, security::RiskCategory::InputSchema, security::RiskSeverity::Low));
}

TEST_CASE("Resource exposure rules flag local secret resources", "[security][rules]")
{
  mcp::McpResource resource;
  resource.uri = QStringLiteral("file:///home/user/.env");
  resource.name = QStringLiteral("env-file");
  resource.description = QStringLiteral("Private token configuration");
  resource.size = 12 * 1024 * 1024;

  const auto findings = security::evaluate_resource_exposure(resource);

  REQUIRE(has_category(findings, security::RiskCategory::ResourceExposure, security::RiskSeverity::High));
  REQUIRE(has_category(findings, security::RiskCategory::ResourceExposure, security::RiskSeverity::Critical));
  REQUIRE(has_category(findings, security::RiskCategory::ResourceExposure, security::RiskSeverity::Low));
}

TEST_CASE("Default security rule set covers profile, tools, schemas, and resources", "[security][rules]")
{
  security::RuleEngine engine;
  engine.add_rules(security::default_security_rules());

  config::ServerProfile profile;
  profile.transport = config::ServerTransportKind::Stdio;
  profile.command = QStringLiteral("bash");
  profile.args = QStringList{QStringLiteral("-lc"), QStringLiteral("rm -rf /tmp/demo")};
  profile.env.insert(QStringLiteral("API_KEY"), QStringLiteral("sk-demo"));

  const mcp::McpTool tool{
      QStringLiteral("exec_command"),
      QStringLiteral("Run a command"),
      QJsonObject{
          {QStringLiteral("type"), QStringLiteral("object")},
          {QStringLiteral("properties"), QJsonObject{
              {QStringLiteral("command"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}},
          }},
      },
  };

  mcp::McpResource resource;
  resource.uri = QStringLiteral("file:///home/user/.ssh/id_rsa");
  resource.name = QStringLiteral("private-key");

  const auto findings = engine.evaluate(security::RuleInput{profile, {tool}, {resource}});

  REQUIRE(engine.count() == 6);
  REQUIRE(has_category(findings, security::RiskCategory::ProfileCommand, security::RiskSeverity::High));
  REQUIRE(has_category(findings, security::RiskCategory::ProfileEnvironment, security::RiskSeverity::High));
  REQUIRE(has_category(findings, security::RiskCategory::ToolMetadata, security::RiskSeverity::High));
  REQUIRE(has_category(findings, security::RiskCategory::InputSchema, security::RiskSeverity::High));
  REQUIRE(has_category(findings, security::RiskCategory::ResourceExposure, security::RiskSeverity::Critical));
}
