#include "McpJson.hpp"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QVariant>

#include <initializer_list>
#include <optional>
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

[[nodiscard]] QString string_value(const QJsonObject& object, const QString& key, const QString& fallback = {})
{
  const auto value = object.value(key);
  return value.isString() ? value.toString() : fallback;
}

[[nodiscard]] QString first_string_value(const QJsonObject& object, std::initializer_list<const char*> keys)
{
  for (const auto* key : keys) {
    const auto value = object.value(QString::fromUtf8(key));
    if (value.isString()) {
      return value.toString();
    }
  }

  return {};
}

[[nodiscard]] QStringList string_list_value(
    const QJsonObject& object,
    const QString& key,
    const QString& server_name,
    ProfileValidationResult& validation)
{
  QStringList values;
  const auto array_value = object.value(key);
  if (array_value.isUndefined()) {
    return values;
  }

  if (!array_value.isArray()) {
    add_issue(
        validation,
        ProfileValidationSeverity::Error,
        QStringLiteral("mcpServers.%1.%2").arg(server_name, key),
        QStringLiteral("%1 must be an array of strings.").arg(key));
    return values;
  }

  const auto array = array_value.toArray();
  for (const auto& item : array) {
    if (!item.isString()) {
      add_issue(
          validation,
          ProfileValidationSeverity::Warning,
          QStringLiteral("mcpServers.%1.%2").arg(server_name, key),
          QStringLiteral("Non-string value in %1 was ignored.").arg(key));
      continue;
    }

    values.push_back(item.toString());
  }

  return values;
}

[[nodiscard]] QMap<QString, QString> string_map_value(
    const QJsonObject& object,
    const QString& key,
    const QString& server_name,
    ProfileValidationResult& validation)
{
  QMap<QString, QString> values;
  const auto object_value = object.value(key);
  if (object_value.isUndefined()) {
    return values;
  }

  if (!object_value.isObject()) {
    add_issue(
        validation,
        ProfileValidationSeverity::Error,
        QStringLiteral("mcpServers.%1.%2").arg(server_name, key),
        QStringLiteral("%1 must be an object.").arg(key));
    return values;
  }

  const auto map_object = object_value.toObject();
  for (auto it = map_object.constBegin(); it != map_object.constEnd(); ++it) {
    if (!it.value().isString()) {
      add_issue(
          validation,
          ProfileValidationSeverity::Warning,
          QStringLiteral("mcpServers.%1.%2.%3").arg(server_name, key, it.key()),
          QStringLiteral("Non-string value for %1 was converted to text.").arg(it.key()));
    }

    values.insert(it.key(), it.value().toVariant().toString());
  }

  return values;
}

[[nodiscard]] std::optional<qint64> timeout_value(const QJsonObject& object, ProfileValidationResult& validation, const QString& server_name)
{
  auto value = object.value(QStringLiteral("timeoutMs"));
  if (value.isUndefined()) {
    value = object.value(QStringLiteral("timeout_ms"));
  }
  if (value.isUndefined()) {
    return std::nullopt;
  }

  bool ok = false;
  const auto timeout = value.toVariant().toLongLong(&ok);
  if (!ok) {
    add_issue(
        validation,
        ProfileValidationSeverity::Error,
        QStringLiteral("mcpServers.%1.timeoutMs").arg(server_name),
        QStringLiteral("timeoutMs must be a number."));
    return std::nullopt;
  }

  return timeout;
}

[[nodiscard]] ServerTransportKind transport_kind_from_server(const QJsonObject& object)
{
  const auto transport = first_string_value(object, {"transport", "type"}).toLower();
  if (transport.contains(QStringLiteral("http")) || object.contains(QStringLiteral("url"))
      || object.contains(QStringLiteral("endpointUrl")) || object.contains(QStringLiteral("endpoint_url"))) {
    return ServerTransportKind::StreamableHttp;
  }

  return ServerTransportKind::Stdio;
}

[[nodiscard]] QJsonArray args_to_json(const QStringList& args)
{
  QJsonArray array;
  for (const auto& arg : args) {
    array.push_back(arg);
  }

  return array;
}

[[nodiscard]] QJsonObject map_to_json(const QMap<QString, QString>& values)
{
  QJsonObject object;
  for (auto it = values.cbegin(); it != values.cend(); ++it) {
    object.insert(it.key(), it.value());
  }

  return object;
}

[[nodiscard]] QString export_key_for_profile(const ServerProfile& profile, int fallback_index)
{
  if (!profile.name.trimmed().isEmpty()) {
    return profile.name.trimmed();
  }
  if (!profile.id.trimmed().isEmpty()) {
    return profile.id.trimmed();
  }
  if (!profile.command.trimmed().isEmpty()) {
    return profile.command.trimmed();
  }

  return QStringLiteral("server-%1").arg(fallback_index + 1);
}

void append_profile_validation(ProfileValidationResult& aggregate, const QString& profile_name, const ProfileValidationResult& validation)
{
  for (const auto& issue : validation.issues) {
    add_issue(
        aggregate,
        issue.severity,
        QStringLiteral("mcpServers.%1.%2").arg(profile_name, issue.field),
        QStringLiteral("%1: %2").arg(profile_name, issue.message));
  }
}

} // namespace

bool McpJsonParseResult::ok() const
{
  return validation.ok() && !profiles.empty();
}

QString McpJsonParseResult::summary() const
{
  if (profiles.empty() && validation.issues.empty()) {
    return QStringLiteral("No MCP server profiles were found.");
  }

  const auto validation_summary = validation.summary();
  return QStringLiteral("%1 profile(s) parsed.\n%2").arg(profiles.size()).arg(validation_summary);
}

McpJsonParseResult parse_mcp_json(const QString& content)
{
  McpJsonParseResult result;

  QJsonParseError parse_error;
  const auto document = QJsonDocument::fromJson(content.toUtf8(), &parse_error);
  if (parse_error.error != QJsonParseError::NoError) {
    add_issue(
        result.validation,
        ProfileValidationSeverity::Error,
        QStringLiteral("mcp.json"),
        QStringLiteral("JSON parse error at offset %1: %2").arg(parse_error.offset).arg(parse_error.errorString()));
    return result;
  }

  if (!document.isObject()) {
    add_issue(
        result.validation,
        ProfileValidationSeverity::Error,
        QStringLiteral("mcp.json"),
        QStringLiteral("mcp.json root must be a JSON object."));
    return result;
  }

  const auto root = document.object();
  const auto servers_value = root.value(QStringLiteral("mcpServers"));
  if (!servers_value.isObject()) {
    add_issue(
        result.validation,
        ProfileValidationSeverity::Error,
        QStringLiteral("mcpServers"),
        QStringLiteral("mcp.json must contain an mcpServers object."));
    return result;
  }

  const auto servers = servers_value.toObject();
  for (auto it = servers.constBegin(); it != servers.constEnd(); ++it) {
    const auto server_name = it.key();
    if (!it.value().isObject()) {
      add_issue(
          result.validation,
          ProfileValidationSeverity::Error,
          QStringLiteral("mcpServers.%1").arg(server_name),
          QStringLiteral("Server entry must be an object."));
      continue;
    }

    const auto server = it.value().toObject();
    ServerProfile profile;
    profile.id = server_name;
    profile.name = server_name;
    profile.transport = transport_kind_from_server(server);
    profile.command = string_value(server, QStringLiteral("command"));
    profile.args = string_list_value(server, QStringLiteral("args"), server_name, result.validation);
    profile.cwd = first_string_value(server, {"cwd", "workingDirectory", "working_directory"});
    profile.env = string_map_value(server, QStringLiteral("env"), server_name, result.validation);
    profile.timeout_ms = timeout_value(server, result.validation, server_name);
    profile.endpoint_url = first_string_value(server, {"url", "endpointUrl", "endpoint_url"});
    profile.http_headers = string_map_value(server, QStringLiteral("headers"), server_name, result.validation);
    if (profile.http_headers.empty()) {
      profile.http_headers = string_map_value(server, QStringLiteral("httpHeaders"), server_name, result.validation);
    }
    profile.protocol_version = first_string_value(server, {"protocolVersion", "protocol_version"});
    if (profile.protocol_version.trimmed().isEmpty()) {
      profile.protocol_version = default_mcp_protocol_version();
    }

    append_profile_validation(result.validation, server_name, validate_profile(profile));
    result.profiles.push_back(std::move(profile));
  }

  if (result.profiles.empty()) {
    add_issue(
        result.validation,
        ProfileValidationSeverity::Error,
        QStringLiteral("mcpServers"),
        QStringLiteral("mcpServers does not contain any importable server entries."));
  }

  return result;
}

QString export_mcp_json(const QVector<ServerProfile>& profiles)
{
  QJsonObject servers;
  for (int index = 0; index < profiles.size(); ++index) {
    const auto& profile = profiles.at(index);
    QJsonObject server;

    if (profile.transport == ServerTransportKind::StreamableHttp) {
      server.insert(QStringLiteral("type"), QStringLiteral("streamable-http"));
      server.insert(QStringLiteral("url"), profile.endpoint_url);
      if (!profile.http_headers.empty()) {
        server.insert(QStringLiteral("headers"), map_to_json(profile.http_headers));
      }
    } else {
      server.insert(QStringLiteral("command"), profile.command);
      if (!profile.args.empty()) {
        server.insert(QStringLiteral("args"), args_to_json(profile.args));
      }
      if (!profile.env.empty()) {
        server.insert(QStringLiteral("env"), map_to_json(profile.env));
      }
      if (!profile.cwd.trimmed().isEmpty()) {
        server.insert(QStringLiteral("cwd"), profile.cwd);
      }
    }

    if (profile.timeout_ms.has_value()) {
      server.insert(QStringLiteral("timeoutMs"), static_cast<double>(*profile.timeout_ms));
    }
    if (!profile.protocol_version.trimmed().isEmpty()) {
      server.insert(QStringLiteral("protocolVersion"), profile.protocol_version.trimmed());
    }

    servers.insert(export_key_for_profile(profile, index), server);
  }

  const QJsonObject root{{QStringLiteral("mcpServers"), servers}};
  return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

ProfileValidationResult validate_mcp_json_profiles(const QVector<ServerProfile>& profiles)
{
  ProfileValidationResult result;
  if (profiles.empty()) {
    add_issue(
        result,
        ProfileValidationSeverity::Error,
        QStringLiteral("mcpServers"),
        QStringLiteral("At least one profile is required for mcp.json export."));
    return result;
  }

  for (const auto& profile : profiles) {
    append_profile_validation(result, profile.name.trimmed().isEmpty() ? profile.id : profile.name, validate_profile(profile));
  }

  return result;
}

McpJsonParseResult load_mcp_json_file(const QString& path)
{
  McpJsonParseResult result;
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    add_issue(
        result.validation,
        ProfileValidationSeverity::Error,
        path,
        QStringLiteral("Could not open mcp.json: %1").arg(file.errorString()));
    return result;
  }

  return parse_mcp_json(QString::fromUtf8(file.readAll()));
}

bool save_mcp_json_file(const QString& path, const QVector<ServerProfile>& profiles, QString* error_message)
{
  const auto validation = validate_mcp_json_profiles(profiles);
  if (validation.has_errors()) {
    if (error_message != nullptr) {
      *error_message = validation.summary();
    }
    return false;
  }

  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
    if (error_message != nullptr) {
      *error_message = file.errorString();
    }
    return false;
  }

  const auto bytes = export_mcp_json(profiles).toUtf8();
  if (file.write(bytes) != bytes.size()) {
    if (error_message != nullptr) {
      *error_message = QStringLiteral("Could not write the complete mcp.json file.");
    }
    return false;
  }

  return true;
}

} // namespace config
