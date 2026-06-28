#pragma once

#include "McpPrompt.hpp"
#include "McpResource.hpp"
#include "McpTool.hpp"

#include <QJsonObject>
#include <QString>

#include <vector>

namespace compare {

struct ServerSnapshot
{
  QString name;
  QJsonObject capabilities;
  std::vector<mcp::McpTool> tools;
  std::vector<mcp::McpResource> resources;
  std::vector<mcp::McpPrompt> prompts;

  friend bool operator==(const ServerSnapshot& lhs, const ServerSnapshot& rhs) = default;
};

} // namespace compare
