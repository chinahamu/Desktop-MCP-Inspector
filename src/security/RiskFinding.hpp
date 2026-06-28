#pragma once

#include <QString>

#include <vector>

namespace security {

enum class RiskSeverity {
  Info,
  Low,
  Medium,
  High,
  Critical,
};

enum class RiskCategory {
  ProfileCommand,
  ProfileEnvironment,
  HttpEndpoint,
  ToolMetadata,
  InputSchema,
  ResourceExposure,
};

struct RiskFinding
{
  QString id;
  RiskSeverity severity = RiskSeverity::Info;
  RiskCategory category = RiskCategory::ProfileCommand;
  QString title;
  QString detail;
  QString recommendation;
  QString subject;

  friend bool operator==(const RiskFinding& lhs, const RiskFinding& rhs) = default;
};

struct RiskScore
{
  int value = 100;
  RiskSeverity severity = RiskSeverity::Info;
  QString summary;

  friend bool operator==(const RiskScore& lhs, const RiskScore& rhs) = default;
};

[[nodiscard]] QString to_string(RiskSeverity severity);
[[nodiscard]] QString to_string(RiskCategory category);
[[nodiscard]] int severity_weight(RiskSeverity severity);
[[nodiscard]] RiskSeverity max_severity(const std::vector<RiskFinding>& findings);
[[nodiscard]] RiskScore calculate_risk_score(const std::vector<RiskFinding>& findings);

} // namespace security
