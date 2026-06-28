#include "DiffMarkdownWriter.hpp"
#include "ServerDiff.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include <algorithm>
#include <optional>
#include <vector>

namespace {

[[nodiscard]] bool contains_entry(
    const std::vector<compare::DiffEntry>& entries,
    compare::DiffSection section,
    compare::DiffBreakingLevel level,
    const QString& identifier)
{
  return std::any_of(entries.begin(), entries.end(), [&](const compare::DiffEntry& entry) {
    return entry.section == section && entry.breaking == level && entry.identifier == identifier;
  });
}

} // namespace

TEST_CASE("capability diff detects added changed and removed capabilities")
{
  const QJsonObject before{
      {QStringLiteral("tools"), QJsonObject{{QStringLiteral("listChanged"), false}}},
      {QStringLiteral("resources"), QJsonObject{{QStringLiteral("subscribe"), true}}},
  };
  const QJsonObject after{
      {QStringLiteral("tools"), QJsonObject{{QStringLiteral("listChanged"), true}}},
      {QStringLiteral("prompts"), QJsonObject{{QStringLiteral("listChanged"), true}}},
  };

  const auto entries = compare::diff_capabilities(before, after);

  CHECK(entries.size() == 3);
  CHECK(contains_entry(entries, compare::DiffSection::Capability, compare::DiffBreakingLevel::Potential, QStringLiteral("tools")));
  CHECK(contains_entry(entries, compare::DiffSection::Capability, compare::DiffBreakingLevel::Breaking, QStringLiteral("resources")));
  CHECK(contains_entry(entries, compare::DiffSection::Capability, compare::DiffBreakingLevel::None, QStringLiteral("prompts")));
}

TEST_CASE("tool diff separates metadata and breaking input schema changes")
{
  const mcp::McpTool before{
      QStringLiteral("search"),
      QStringLiteral("Search documents."),
      QJsonObject{
          {QStringLiteral("type"), QStringLiteral("object")},
          {QStringLiteral("properties"), QJsonObject{
              {QStringLiteral("query"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}},
          }},
      },
  };
  const mcp::McpTool after{
      QStringLiteral("search"),
      QStringLiteral("Search indexed documents."),
      QJsonObject{
          {QStringLiteral("type"), QStringLiteral("object")},
          {QStringLiteral("properties"), QJsonObject{
              {QStringLiteral("query"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}},
          }},
          {QStringLiteral("required"), QJsonArray{QStringLiteral("query")}},
      },
  };

  const auto entries = compare::diff_tools({before}, {after});

  CHECK(entries.size() == 2);
  CHECK(contains_entry(entries, compare::DiffSection::Tool, compare::DiffBreakingLevel::Potential, QStringLiteral("search")));
  CHECK(contains_entry(entries, compare::DiffSection::InputSchema, compare::DiffBreakingLevel::Breaking, QStringLiteral("search")));
}

TEST_CASE("server diff summarizes resources prompts and breaking changes")
{
  compare::ServerSnapshot before;
  before.name = QStringLiteral("before");
  before.resources.push_back(mcp::McpResource{
      QStringLiteral("file:///legacy.txt"),
      QStringLiteral("legacy"),
      QStringLiteral("Legacy"),
      QStringLiteral("Legacy document"),
      QStringLiteral("text/plain"),
      10,
      std::nullopt,
  });
  before.prompts.push_back(mcp::McpPrompt{
      QStringLiteral("summarize"),
      QStringLiteral("Summarize text."),
      std::vector<mcp::McpPromptArgument>{
          mcp::McpPromptArgument{QStringLiteral("text"), QStringLiteral("Text to summarize."), false},
      },
  });

  compare::ServerSnapshot after;
  after.name = QStringLiteral("after");
  after.prompts.push_back(mcp::McpPrompt{
      QStringLiteral("summarize"),
      QStringLiteral("Summarize text."),
      std::vector<mcp::McpPromptArgument>{
          mcp::McpPromptArgument{QStringLiteral("text"), QStringLiteral("Text to summarize."), true},
      },
  });

  const auto diff = compare::diff_server_snapshots(before, after);

  CHECK(diff.summary.total() == 2);
  CHECK(diff.summary.removed == 1);
  CHECK(diff.summary.modified == 1);
  CHECK(diff.has_breaking_changes());
  CHECK(contains_entry(diff.entries, compare::DiffSection::Resource, compare::DiffBreakingLevel::Breaking, QStringLiteral("file:///legacy.txt")));
  CHECK(contains_entry(diff.entries, compare::DiffSection::Prompt, compare::DiffBreakingLevel::Breaking, QStringLiteral("summarize")));
}

TEST_CASE("diff markdown writer exports a readable table")
{
  compare::ServerDiffResult diff;
  diff.before_name = QStringLiteral("before");
  diff.after_name = QStringLiteral("after");
  diff.entries.push_back(compare::DiffEntry{
      compare::DiffSection::Tool,
      compare::DiffChangeType::Added,
      compare::DiffBreakingLevel::None,
      QStringLiteral("echo"),
      QStringLiteral("Tool added"),
      QStringLiteral("A new tool is available."),
      {},
      QStringLiteral("Echo tool"),
  });
  diff.summary.added = 1;

  const auto markdown = report::DiffMarkdownWriter{}.write(diff);

  CHECK(markdown.contains(QStringLiteral("Desktop MCP Inspector Diff Report")));
  CHECK(markdown.contains(QStringLiteral("| Tool | added | none | echo |")));
}
