#include "RuleEngine.hpp"

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace security {

void RuleEngine::add_rule(RuleDefinition rule)
{
  if (rule.id.trimmed().isEmpty()) {
    throw std::invalid_argument{"security rule id must not be empty"};
  }
  if (!rule.evaluate) {
    throw std::invalid_argument{"security rule must provide an evaluator"};
  }

  const auto duplicate = std::any_of(rules_.begin(), rules_.end(), [&rule](const RuleDefinition& existing) {
    return existing.id == rule.id;
  });
  if (duplicate) {
    throw std::invalid_argument{"security rule id must be unique"};
  }

  rules_.push_back(std::move(rule));
}

void RuleEngine::add_rules(std::vector<RuleDefinition> rules)
{
  for (auto& rule : rules) {
    add_rule(std::move(rule));
  }
}

std::vector<RiskFinding> RuleEngine::evaluate(const RuleInput& input) const
{
  std::vector<RiskFinding> findings;
  for (const auto& rule : rules_) {
    auto rule_findings = rule.evaluate(input);
    findings.insert(
        findings.end(),
        std::make_move_iterator(rule_findings.begin()),
        std::make_move_iterator(rule_findings.end()));
  }
  return findings;
}

bool RuleEngine::empty() const
{
  return rules_.empty();
}

std::size_t RuleEngine::count() const
{
  return rules_.size();
}

} // namespace security
