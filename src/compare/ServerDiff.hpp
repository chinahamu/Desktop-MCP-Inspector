#pragma once

#include "ServerSnapshot.hpp"

#include <QString>
#include <QtTypes>

#include <vector>

namespace compare {

enum class DiffSection {
  Capability,
  Tool,
  InputSchema,
  Resource,
  Prompt,
};

enum class DiffChangeType {
  Added,
  Removed,
  Modified,
};

enum class DiffBreakingLevel {
  None,
  Potential,
  Breaking,
};

struct DiffEntry
{
  DiffSection section = DiffSection::Capability;
  DiffChangeType change = DiffChangeType::Modified;
  DiffBreakingLevel breaking = DiffBreakingLevel::None;
  QString identifier;
  QString title;
  QString detail;
  QString before_value;
  QString after_value;

  friend bool operator==(const DiffEntry& lhs, const DiffEntry& rhs) = default;
};

struct ServerDiffSummary
{
  qsizetype added = 0;
  qsizetype removed = 0;
  qsizetype modified = 0;
  qsizetype breaking = 0;
  qsizetype potential = 0;

  [[nodiscard]] qsizetype total() const;
  [[nodiscard]] bool has_breaking_changes() const;

  friend bool operator==(const ServerDiffSummary& lhs, const ServerDiffSummary& rhs) = default;
};

struct ServerDiffResult
{
  QString before_name;
  QString after_name;
  ServerDiffSummary summary;
  std::vector<DiffEntry> entries;

  [[nodiscard]] bool has_changes() const;
  [[nodiscard]] bool has_breaking_changes() const;

  friend bool operator==(const ServerDiffResult& lhs, const ServerDiffResult& rhs) = default;
};

[[nodiscard]] QString to_string(DiffSection section);
[[nodiscard]] QString to_string(DiffChangeType change);
[[nodiscard]] QString to_string(DiffBreakingLevel breaking);

[[nodiscard]] std::vector<DiffEntry> diff_capabilities(
    const QJsonObject& before,
    const QJsonObject& after);
[[nodiscard]] std::vector<DiffEntry> diff_tools(
    const std::vector<mcp::McpTool>& before,
    const std::vector<mcp::McpTool>& after);
[[nodiscard]] std::vector<DiffEntry> diff_resources(
    const std::vector<mcp::McpResource>& before,
    const std::vector<mcp::McpResource>& after);
[[nodiscard]] std::vector<DiffEntry> diff_prompts(
    const std::vector<mcp::McpPrompt>& before,
    const std::vector<mcp::McpPrompt>& after);
[[nodiscard]] ServerDiffResult diff_server_snapshots(
    const ServerSnapshot& before,
    const ServerSnapshot& after);

} // namespace compare
