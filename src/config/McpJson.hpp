#pragma once

#include "ProfileValidation.hpp"
#include "ServerProfile.hpp"

#include <QString>
#include <QVector>

namespace config {

struct McpJsonParseResult
{
  QVector<ServerProfile> profiles;
  ProfileValidationResult validation;

  [[nodiscard]] bool ok() const;
  [[nodiscard]] QString summary() const;
};

[[nodiscard]] McpJsonParseResult parse_mcp_json(const QString& content);
[[nodiscard]] QString export_mcp_json(const QVector<ServerProfile>& profiles);
[[nodiscard]] ProfileValidationResult validate_mcp_json_profiles(const QVector<ServerProfile>& profiles);
[[nodiscard]] McpJsonParseResult load_mcp_json_file(const QString& path);
[[nodiscard]] bool save_mcp_json_file(const QString& path, const QVector<ServerProfile>& profiles, QString* error_message = nullptr);

} // namespace config
