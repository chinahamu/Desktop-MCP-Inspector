#pragma once

#include "ServerProfile.hpp"

#include <QString>
#include <QStringList>
#include <QVector>

namespace config {

enum class ProfileValidationSeverity {
  Warning,
  Error,
};

struct ProfileValidationIssue
{
  ProfileValidationSeverity severity = ProfileValidationSeverity::Error;
  QString field;
  QString message;

  friend bool operator==(const ProfileValidationIssue& lhs, const ProfileValidationIssue& rhs) = default;
};

struct ProfileValidationResult
{
  QVector<ProfileValidationIssue> issues;

  [[nodiscard]] bool has_errors() const;
  [[nodiscard]] bool ok() const;
  [[nodiscard]] QStringList error_messages() const;
  [[nodiscard]] QString summary() const;
};

[[nodiscard]] bool is_valid_env_key(const QString& key);
[[nodiscard]] ProfileValidationResult validate_profile(const ServerProfile& profile);

} // namespace config
