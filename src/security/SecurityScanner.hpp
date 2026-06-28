#pragma once

#include "RiskFinding.hpp"
#include "RuleEngine.hpp"

#include <cstddef>

namespace security {

using SecurityScanInput = RuleInput;

struct SecurityScanResult
{
  std::vector<RiskFinding> findings;
  RiskScore score;
  std::size_t rule_count = 0;
  bool passed = true;

  friend bool operator==(const SecurityScanResult& lhs, const SecurityScanResult& rhs) = default;
};

class SecurityScanner final
{
public:
  SecurityScanner();
  explicit SecurityScanner(RuleEngine rule_engine);

  [[nodiscard]] static SecurityScanner default_scanner();

  [[nodiscard]] SecurityScanResult scan(const SecurityScanInput& input) const;
  [[nodiscard]] const RuleEngine& rule_engine() const;
  [[nodiscard]] std::size_t rule_count() const;

private:
  RuleEngine rule_engine_;
};

} // namespace security
