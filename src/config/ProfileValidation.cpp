#include "ProfileValidation.hpp"

#include <QRegularExpression>
#include <QUrl>

#include <utility>

namespace config {
namespace {

void add_issue(
    ProfileValidationResult& result,
    ProfileValidationSeverity severity,
    QString field,
    QString message)
{
  result.issues.push_back(ProfileValidationIssue{
      severity,
      std::move(field),
      std::move(message),
  });
}

[[nodiscard]] bool is_valid_http_header_name(const QString& name)
{
  if (name.trimmed() != name || name.isEmpty()) {
    return false;
  }

  static const QString separators = QStringLiteral("()<>@,;:/[]?={}");
  for (const auto character : name) {
    const auto codepoint = character.unicode();
    if (codepoint <= 32 || codepoint >= 127 || character == QChar('\\') || character == QChar('"')
        || separators.contains(character)) {
      return false;
    }
  }

  return true;
}

void validate_stdio_profile(const ServerProfile& profile, ProfileValidationResult& result)
{
  if (profile.command.trimmed().isEmpty()) {
    add_issue(
        result,
        ProfileValidationSeverity::Error,
        QStringLiteral("command"),
        QStringLiteral("Command must not be empty."));
  }
}

void validate_streamable_http_profile(const ServerProfile& profile, ProfileValidationResult& result)
{
  const auto endpoint = profile.endpoint_url.trimmed();
  if (endpoint.isEmpty()) {
    add_issue(
        result,
        ProfileValidationSeverity::Error,
        QStringLiteral("endpoint_url"),
        QStringLiteral("HTTP endpoint URL must not be empty."));
  } else {
    const QUrl url(endpoint, QUrl::StrictMode);
    const auto scheme = url.scheme().toLower();
    if (!url.isValid() || url.host().isEmpty() || (scheme != QStringLiteral("http") && scheme != QStringLiteral("https"))) {
      add_issue(
          result,
          ProfileValidationSeverity::Error,
          QStringLiteral("endpoint_url"),
          QStringLiteral("HTTP endpoint URL must be a valid http:// or https:// URL."));
    }
  }

  if (profile.protocol_version.trimmed().isEmpty()) {
    add_issue(
        result,
        ProfileValidationSeverity::Warning,
        QStringLiteral("protocol_version"),
        QStringLiteral("MCP protocol version header will be omitted."));
  }

  for (auto it = profile.http_headers.cbegin(); it != profile.http_headers.cend(); ++it) {
    if (!is_valid_http_header_name(it.key())) {
      add_issue(
          result,
          ProfileValidationSeverity::Error,
          QStringLiteral("http_headers"),
          QStringLiteral("HTTP header name '%1' is invalid.").arg(it.key()));
    }
  }
}

} // namespace

bool ProfileValidationResult::has_errors() const
{
  for (const auto& issue : issues) {
    if (issue.severity == ProfileValidationSeverity::Error) {
      return true;
    }
  }

  return false;
}

bool ProfileValidationResult::ok() const
{
  return !has_errors();
}

QStringList ProfileValidationResult::error_messages() const
{
  QStringList messages;
  for (const auto& issue : issues) {
    if (issue.severity == ProfileValidationSeverity::Error) {
      messages.push_back(issue.message);
    }
  }

  return messages;
}

QString ProfileValidationResult::summary() const
{
  if (issues.empty()) {
    return QStringLiteral("Profile validation passed.");
  }

  QStringList messages;
  for (const auto& issue : issues) {
    const auto prefix = issue.severity == ProfileValidationSeverity::Error
        ? QStringLiteral("Error")
        : QStringLiteral("Warning");
    messages.push_back(QStringLiteral("%1: %2").arg(prefix, issue.message));
  }

  return messages.join(QStringLiteral("\n"));
}

bool is_valid_env_key(const QString& key)
{
  static const QRegularExpression env_key_pattern(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*$"));
  return env_key_pattern.match(key).hasMatch();
}

ProfileValidationResult validate_profile(const ServerProfile& profile)
{
  ProfileValidationResult result;

  if (profile.name.trimmed().isEmpty()) {
    add_issue(
        result,
        ProfileValidationSeverity::Warning,
        QStringLiteral("name"),
        QStringLiteral("Profile name is empty."));
  }

  switch (profile.transport) {
    case ServerTransportKind::Stdio:
      validate_stdio_profile(profile, result);
      break;
    case ServerTransportKind::StreamableHttp:
      validate_streamable_http_profile(profile, result);
      break;
  }

  if (profile.timeout_ms.has_value() && *profile.timeout_ms <= 0) {
    add_issue(
        result,
        ProfileValidationSeverity::Error,
        QStringLiteral("timeout_ms"),
        QStringLiteral("Timeout must be a positive number of milliseconds."));
  }

  for (auto it = profile.env.cbegin(); it != profile.env.cend(); ++it) {
    if (!is_valid_env_key(it.key())) {
      add_issue(
          result,
          ProfileValidationSeverity::Error,
          QStringLiteral("env"),
          QStringLiteral("Environment variable key '%1' is invalid.").arg(it.key()));
    }
  }

  return result;
}

} // namespace config
