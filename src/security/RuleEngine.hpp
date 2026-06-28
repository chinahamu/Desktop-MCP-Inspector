#pragma once

#include "RiskFinding.hpp"

#include "McpResource.hpp"
#include "McpTool.hpp"
#include "ServerProfile.hpp"

#include <functional>
#include <optional>
#include <vector>

namespace security {

struct RuleInput
{
  std::optional<config::ServerProfile> profile;
  std::vector<mcp::McpTool> tools;
  std::vector<mcp::McpResource> resources;
};

struct RuleDefinition
{
  QString id;
  QString name;
  RiskCategory category = RiskCategory::ProfileCommand;
  std::function<std::vector<RiskFinding>(const RuleInput&)> evaluate;
};

class RuleEngine
{
public:
  void add_rule(RuleDefinition rule);
  void add_rules(std::vector<RuleDefinition> rules);

  [[nodiscard]] std::vector<RiskFinding> evaluate(const RuleInput& input) const;
  [[nodiscard]] bool empty() const;
  [[nodiscard]] std::size_t count() const;

private:
  std::vector<RuleDefinition> rules_;
};

} // namespace security
