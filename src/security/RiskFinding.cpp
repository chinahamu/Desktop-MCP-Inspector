#include "RiskFinding.hpp"

#include <algorithm>
#include <set>

namespace security {

QString to_string(RiskSeverity severity)
{
  switch (severity) {
  case RiskSeverity::Info:
    return QStringLiteral("info");
  case RiskSeverity::Low:
    return QStringLiteral("low");
  case RiskSeverity::Medium:
    return QStringLiteral("medium");
  case RiskSeverity::High:
    return QStringLiteral("high");
  case RiskSeverity::Critical:
    return QStringLiteral("critical");
  }

  return QStringLiteral("unknown");
}

QString to_string(RiskCategory category)
{
  switch (category) {
  case RiskCategory::ProfileCommand:
    return QStringLiteral("profile.command");
  case RiskCategory::ProfileEnvironment:
    return QStringLiteral("profile.environment");
  case RiskCategory::HttpEndpoint:
    return QStringLiteral("http.endpoint");
  case RiskCategory::ToolMetadata:
    return QStringLiteral("tool.metadata");
  case RiskCategory::InputSchema:
    return QStringLiteral("tool.inputSchema");
  case RiskCategory::ResourceExposure:
    return QStringLiteral("resource.exposure");
  }

  return QStringLiteral("unknown");
}

int severity_weight(RiskSeverity severity)
{
  switch (severity) {
  case RiskSeverity::Info:
    return 0;
  case RiskSeverity::Low:
    return 5;
  case RiskSeverity::Medium:
    return 12;
  case RiskSeverity::High:
    return 25;
  case RiskSeverity::Critical:
    return 40;
  }

  return 0;
}

RiskSeverity max_severity(const std::vector<RiskFinding>& findings)
{
  auto severity = RiskSeverity::Info;
  for (const auto& finding : findings) {
    if (severity_weight(finding.severity) > severity_weight(severity)) {
      severity = finding.severity;
    }
  }
  return severity;
}

RiskScore calculate_risk_score(const std::vector<RiskFinding>& findings)
{
  RiskScore score;
  score.severity = max_severity(findings);

  if (findings.empty()) {
    score.value = 100;
    score.summary = QStringLiteral("No security findings");
    return score;
  }

  auto weighted_penalty = 0;
  std::set<RiskCategory> categories;
  for (const auto& finding : findings) {
    weighted_penalty += severity_weight(finding.severity);
    categories.insert(finding.category);
  }

  const auto diversity_penalty = std::min(static_cast<int>(categories.size()) * 2, 10);
  const auto volume_penalty = std::min(std::max(static_cast<int>(findings.size()) - 3, 0) * 3, 15);
  const auto total_penalty = weighted_penalty + diversity_penalty + volume_penalty;

  score.value = std::clamp(100 - total_penalty, 0, 100);
  score.summary = QStringLiteral("%1 security finding(s), score %2/100, highest severity: %3")
                      .arg(static_cast<int>(findings.size()))
                      .arg(score.value)
                      .arg(to_string(score.severity));
  return score;
}

} // namespace security
