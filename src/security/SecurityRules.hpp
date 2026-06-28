#pragma once

#include "RuleEngine.hpp"

namespace security {

[[nodiscard]] std::vector<RiskFinding> evaluate_profile_command(const config::ServerProfile& profile);
[[nodiscard]] std::vector<RiskFinding> evaluate_profile_environment(const config::ServerProfile& profile);
[[nodiscard]] std::vector<RiskFinding> evaluate_http_endpoint(const config::ServerProfile& profile);
[[nodiscard]] std::vector<RiskFinding> evaluate_tool_metadata(const mcp::McpTool& tool);
[[nodiscard]] std::vector<RiskFinding> evaluate_input_schema(const mcp::McpTool& tool);
[[nodiscard]] std::vector<RiskFinding> evaluate_resource_exposure(const mcp::McpResource& resource);

[[nodiscard]] std::vector<RuleDefinition> command_argument_rules();
[[nodiscard]] std::vector<RuleDefinition> environment_secret_rules();
[[nodiscard]] std::vector<RuleDefinition> http_endpoint_rules();
[[nodiscard]] std::vector<RuleDefinition> tool_metadata_rules();
[[nodiscard]] std::vector<RuleDefinition> input_schema_rules();
[[nodiscard]] std::vector<RuleDefinition> resource_exposure_rules();
[[nodiscard]] std::vector<RuleDefinition> default_security_rules();

} // namespace security
