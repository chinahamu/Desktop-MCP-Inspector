#include "SecurityScanner.hpp"

#include "SecurityRules.hpp"

#include <utility>

namespace security {
namespace {

[[nodiscard]] bool has_blocking_finding(const std::vector<RiskFinding>& findings)
{
  for (const auto& finding : findings) {
    if (finding.severity == RiskSeverity::High || finding.severity == RiskSeverity::Critical) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] RuleEngine make_default_rule_engine()
{
  RuleEngine engine;
  engine.add_rules(default_security_rules());
  return engine;
}

} // namespace

SecurityScanner::SecurityScanner()
  : rule_engine_(make_default_rule_engine())
{
}

SecurityScanner::SecurityScanner(RuleEngine rule_engine)
  : rule_engine_(std::move(rule_engine))
{
}

SecurityScanner SecurityScanner::default_scanner()
{
  return SecurityScanner{make_default_rule_engine()};
}

SecurityScanResult SecurityScanner::scan(const SecurityScanInput& input) const
{
  SecurityScanResult result;
  result.findings = rule_engine_.evaluate(input);
  result.score = calculate_risk_score(result.findings);
  result.rule_count = rule_engine_.count();
  result.passed = result.score.value >= 70 && !has_blocking_finding(result.findings);
  return result;
}

const RuleEngine& SecurityScanner::rule_engine() const
{
  return rule_engine_;
}

std::size_t SecurityScanner::rule_count() const
{
  return rule_engine_.count();
}

} // namespace security
