#include "ProfileStore.hpp"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>
#include <QVariant>

#include <utility>

namespace config {
namespace {

constexpr auto kProfilesTableSql = R"SQL(
CREATE TABLE IF NOT EXISTS profiles (
  id TEXT PRIMARY KEY NOT NULL,
  name TEXT NOT NULL,
  transport TEXT NOT NULL,
  command TEXT NOT NULL,
  args_json TEXT NOT NULL,
  env_json TEXT NOT NULL,
  cwd TEXT NOT NULL,
  timeout_ms INTEGER,
  is_recent INTEGER NOT NULL DEFAULT 0,
  updated_at TEXT NOT NULL,
  endpoint_url TEXT NOT NULL DEFAULT '',
  http_headers_json TEXT NOT NULL DEFAULT '{}',
  protocol_version TEXT NOT NULL DEFAULT ''
)
)SQL";

[[nodiscard]] QString transport_to_string(ServerTransportKind kind)
{
  switch (kind) {
    case ServerTransportKind::Stdio:
      return QStringLiteral("stdio");
    case ServerTransportKind::StreamableHttp:
      return QStringLiteral("streamable_http");
  }

  return QStringLiteral("stdio");
}

[[nodiscard]] ServerTransportKind transport_from_string(const QString& value)
{
  if (value == QStringLiteral("streamable_http")) {
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

[[nodiscard]] QStringList args_from_json(const QString& payload)
{
  QStringList args;
  const auto document = QJsonDocument::fromJson(payload.toUtf8());
  if (!document.isArray()) {
    return args;
  }

  for (const auto& value : document.array()) {
    args.push_back(value.toString());
  }

  return args;
}

[[nodiscard]] QJsonObject map_to_json(const QMap<QString, QString>& values)
{
  QJsonObject object;
  for (auto it = values.cbegin(); it != values.cend(); ++it) {
    object.insert(it.key(), it.value());
  }

  return object;
}

[[nodiscard]] QMap<QString, QString> map_from_json(const QString& payload)
{
  QMap<QString, QString> values;
  const auto document = QJsonDocument::fromJson(payload.toUtf8());
  if (!document.isObject()) {
    return values;
  }

  const auto object = document.object();
  for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
    values.insert(it.key(), it.value().toString());
  }

  return values;
}

[[nodiscard]] QString args_json_text(const QStringList& args)
{
  return QString::fromUtf8(QJsonDocument(args_to_json(args)).toJson(QJsonDocument::Compact));
}

[[nodiscard]] QString map_json_text(const QMap<QString, QString>& values)
{
  return QString::fromUtf8(QJsonDocument(map_to_json(values)).toJson(QJsonDocument::Compact));
}

[[nodiscard]] ServerProfile normalize_profile(ServerProfile profile)
{
  if (profile.id.trimmed().isEmpty()) {
    profile.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  }

  if (profile.name.trimmed().isEmpty()) {
    if (is_streamable_http_profile(profile) && !profile.endpoint_url.trimmed().isEmpty()) {
      profile.name = profile.endpoint_url.trimmed();
    } else {
      profile.name = profile.command.trimmed().isEmpty()
          ? QStringLiteral("Untitled Profile")
          : profile.command.trimmed();
    }
  }

  if (profile.protocol_version.trimmed().isEmpty()) {
    profile.protocol_version = default_mcp_protocol_version();
  }

  return profile;
}

[[nodiscard]] QString sqlite_connection_name()
{
  return QStringLiteral("desktop_mcp_inspector_profiles_%1").arg(QUuid::createUuid().toString(QUuid::Id128));
}

[[nodiscard]] bool open_sqlite_database(const QString& path, const QString& connection_name, QSqlDatabase* database, QString* error_message)
{
  const QFileInfo database_info(path);
  if (!database_info.absolutePath().isEmpty()) {
    QDir().mkpath(database_info.absolutePath());
  }

  *database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection_name);
  database->setDatabaseName(path);
  if (!database->open()) {
    if (error_message != nullptr) {
      *error_message = database->lastError().text();
    }
    return false;
  }

  return true;
}

void close_sqlite_database(const QString& connection_name, QSqlDatabase* database)
{
  database->close();
  *database = QSqlDatabase();
  QSqlDatabase::removeDatabase(connection_name);
}

[[nodiscard]] bool exec_sql(QSqlDatabase& database, const QString& sql, QString* error_message)
{
  QSqlQuery query(database);
  if (query.exec(sql)) {
    return true;
  }

  if (error_message != nullptr) {
    *error_message = query.lastError().text();
  }
  return false;
}

[[nodiscard]] bool sqlite_column_exists(QSqlDatabase& database, const QString& column_name, QString* error_message)
{
  QSqlQuery query(database);
  if (!query.exec(QStringLiteral("PRAGMA table_info(profiles)"))) {
    if (error_message != nullptr) {
      *error_message = query.lastError().text();
    }
    return false;
  }

  while (query.next()) {
    if (query.value(1).toString() == column_name) {
      return true;
    }
  }

  return false;
}

[[nodiscard]] bool ensure_sqlite_column(
    QSqlDatabase& database,
    const QString& column_name,
    const QString& definition,
    QString* error_message)
{
  if (sqlite_column_exists(database, column_name, error_message)) {
    return true;
  }

  return exec_sql(
      database,
      QStringLiteral("ALTER TABLE profiles ADD COLUMN %1 %2").arg(column_name, definition),
      error_message);
}

[[nodiscard]] bool ensure_profiles_schema(QSqlDatabase& database, QString* error_message)
{
  if (!exec_sql(database, QString::fromUtf8(kProfilesTableSql), error_message)) {
    return false;
  }

  return ensure_sqlite_column(database, QStringLiteral("endpoint_url"), QStringLiteral("TEXT NOT NULL DEFAULT ''"), error_message)
      && ensure_sqlite_column(database, QStringLiteral("http_headers_json"), QStringLiteral("TEXT NOT NULL DEFAULT '{}'"), error_message)
      && ensure_sqlite_column(database, QStringLiteral("protocol_version"), QStringLiteral("TEXT NOT NULL DEFAULT ''"), error_message);
}

} // namespace

ProfileStore::ProfileStore() = default;

ProfileStore ProfileStore::in_memory()
{
  return {};
}

ProfileStore ProfileStore::sqlite(QString database_path)
{
  ProfileStore store;
  store.backend_ = Backend::Sqlite;
  store.database_path_ = std::move(database_path);
  return store;
}

QString ProfileStore::default_sqlite_path()
{
  auto base_path = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
  if (base_path.isEmpty()) {
    base_path = QDir::homePath() + QStringLiteral("/.DesktopMCPInspector");
  }

  return QDir(base_path).filePath(QStringLiteral("profiles.sqlite"));
}

ProfileStore::Backend ProfileStore::backend() const
{
  return backend_;
}

const QString& ProfileStore::database_path() const
{
  return database_path_;
}

const QVector<ServerProfile>& ProfileStore::profiles() const
{
  return profiles_;
}

bool ProfileStore::is_empty() const
{
  return profiles_.empty();
}

std::optional<ServerProfile> ProfileStore::profile_by_id(const QString& id) const
{
  const auto index = index_of(id);
  if (index < 0) {
    return std::nullopt;
  }

  return profiles_.at(index);
}

QString ProfileStore::recent_profile_id() const
{
  return recent_profile_id_;
}

void ProfileStore::set_recent_profile_id(QString id)
{
  recent_profile_id_ = std::move(id);
}

std::optional<ServerProfile> ProfileStore::recent_profile() const
{
  return profile_by_id(recent_profile_id_);
}

ServerProfile ProfileStore::upsert(ServerProfile profile)
{
  profile = normalize_profile(std::move(profile));
  const auto index = index_of(profile.id);
  if (index < 0) {
    profiles_.push_back(profile);
  } else {
    profiles_[index] = profile;
  }

  return profile;
}

bool ProfileStore::remove(const QString& id)
{
  const auto index = index_of(id);
  if (index < 0) {
    return false;
  }

  profiles_.removeAt(index);
  if (recent_profile_id_ == id) {
    recent_profile_id_.clear();
  }
  return true;
}

std::optional<ServerProfile> ProfileStore::clone_profile(const QString& id, const QString& name_suffix)
{
  const auto source = profile_by_id(id);
  if (!source.has_value()) {
    return std::nullopt;
  }

  auto clone = *source;
  clone.id.clear();
  clone.name = clone.name + name_suffix;
  return upsert(std::move(clone));
}

void ProfileStore::clear()
{
  profiles_.clear();
  recent_profile_id_.clear();
}

bool ProfileStore::load(QString* error_message)
{
  if (backend_ == Backend::Memory) {
    return true;
  }

  const auto connection_name = sqlite_connection_name();
  QSqlDatabase database;
  if (!open_sqlite_database(database_path_, connection_name, &database, error_message)) {
    close_sqlite_database(connection_name, &database);
    return false;
  }

  bool success = false;
  {
    success = ensure_profiles_schema(database, error_message);
    if (success) {
      QSqlQuery query(database);
      success = query.exec(QStringLiteral(
          "SELECT id, name, transport, command, args_json, env_json, cwd, timeout_ms, is_recent, "
          "endpoint_url, http_headers_json, protocol_version "
          "FROM profiles ORDER BY name COLLATE NOCASE ASC"));
      if (!success) {
        if (error_message != nullptr) {
          *error_message = query.lastError().text();
        }
      } else {
        profiles_.clear();
        recent_profile_id_.clear();

        while (query.next()) {
          ServerProfile profile;
          profile.id = query.value(0).toString();
          profile.name = query.value(1).toString();
          profile.transport = transport_from_string(query.value(2).toString());
          profile.command = query.value(3).toString();
          profile.args = args_from_json(query.value(4).toString());
          profile.env = map_from_json(query.value(5).toString());
          profile.cwd = query.value(6).toString();

          const auto timeout_value = query.value(7);
          if (!timeout_value.isNull()) {
            profile.timeout_ms = timeout_value.toLongLong();
          }

          if (query.value(8).toInt() != 0) {
            recent_profile_id_ = profile.id;
          }

          profile.endpoint_url = query.value(9).toString();
          profile.http_headers = map_from_json(query.value(10).toString());
          profile.protocol_version = query.value(11).toString();
          if (profile.protocol_version.trimmed().isEmpty()) {
            profile.protocol_version = default_mcp_protocol_version();
          }

          profiles_.push_back(std::move(profile));
        }
      }
    }
  }

  close_sqlite_database(connection_name, &database);
  return success;
}

bool ProfileStore::save(QString* error_message) const
{
  if (backend_ == Backend::Memory) {
    return true;
  }

  const auto connection_name = sqlite_connection_name();
  QSqlDatabase database;
  if (!open_sqlite_database(database_path_, connection_name, &database, error_message)) {
    close_sqlite_database(connection_name, &database);
    return false;
  }

  bool success = false;
  {
    success = ensure_profiles_schema(database, error_message);
    if (success && !database.transaction()) {
      success = false;
      if (error_message != nullptr) {
        *error_message = database.lastError().text();
      }
    }

    if (success) {
      success = exec_sql(database, QStringLiteral("DELETE FROM profiles"), error_message);
    }

    if (success) {
      QSqlQuery query(database);
      query.prepare(QStringLiteral(
          "INSERT INTO profiles "
          "(id, name, transport, command, args_json, env_json, cwd, timeout_ms, is_recent, updated_at, "
          "endpoint_url, http_headers_json, protocol_version) "
          "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));

      for (const auto& profile : profiles_) {
        query.addBindValue(profile.id.isNull() ? QString("") : profile.id);
        query.addBindValue(profile.name.isNull() ? QString("") : profile.name);
        query.addBindValue(transport_to_string(profile.transport));
        query.addBindValue(profile.command.isNull() ? QString("") : profile.command);
        query.addBindValue(args_json_text(profile.args));
        query.addBindValue(map_json_text(profile.env));
        query.addBindValue(profile.cwd.isNull() ? QString("") : profile.cwd);
        query.addBindValue(profile.timeout_ms.has_value() ? QVariant::fromValue(*profile.timeout_ms) : QVariant());
        query.addBindValue(profile.id == recent_profile_id_ ? 1 : 0);
        query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
        query.addBindValue(profile.endpoint_url.isNull() ? QString("") : profile.endpoint_url);
        query.addBindValue(map_json_text(profile.http_headers));
        query.addBindValue(profile.protocol_version.isNull() ? QString("") : profile.protocol_version);

        if (!query.exec()) {
          success = false;
          if (error_message != nullptr) {
            *error_message = query.lastError().text();
          }
          break;
        }
      }
    }

    if (success) {
      success = database.commit();
      if (!success && error_message != nullptr) {
        *error_message = database.lastError().text();
      }
    } else {
      database.rollback();
    }
  }

  close_sqlite_database(connection_name, &database);
  return success;
}

int ProfileStore::index_of(const QString& id) const
{
  if (id.isEmpty()) {
    return -1;
  }

  for (int index = 0; index < profiles_.size(); ++index) {
    if (profiles_.at(index).id == id) {
      return index;
    }
  }

  return -1;
}

} // namespace config
