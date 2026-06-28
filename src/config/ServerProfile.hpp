#pragma once

#include <QMap>
#include <QString>
#include <QStringList>
#include <QtTypes>

#include <optional>

namespace config {

[[nodiscard]] inline QString default_mcp_protocol_version()
{
  return QStringLiteral("2025-06-18");
}

enum class ServerTransportKind {
  Stdio,
  StreamableHttp,
};

struct ServerProfile
{
  QString id;
  QString name;
  ServerTransportKind transport = ServerTransportKind::Stdio;
  QString command;
  QStringList args;
  QMap<QString, QString> env;
  QString cwd;
  std::optional<qint64> timeout_ms;
  QString endpoint_url;
  QMap<QString, QString> http_headers;
  QString protocol_version = default_mcp_protocol_version();

  friend bool operator==(const ServerProfile& lhs, const ServerProfile& rhs) = default;
};

[[nodiscard]] inline bool is_stdio_profile(const ServerProfile& profile)
{
  return profile.transport == ServerTransportKind::Stdio;
}

[[nodiscard]] inline bool is_streamable_http_profile(const ServerProfile& profile)
{
  return profile.transport == ServerTransportKind::StreamableHttp;
}

} // namespace config
