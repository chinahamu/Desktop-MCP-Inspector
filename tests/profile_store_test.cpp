#include <catch2/catch_test_macros.hpp>

#include "ProfileStore.hpp"

#include <QDir>
#include <QSqlDatabase>
#include <QTemporaryDir>

TEST_CASE("ProfileStore upserts, remembers recent profile, and clones in memory", "[config][profile]")
{
  auto store = config::ProfileStore::in_memory();

  config::ServerProfile profile;
  profile.name = QStringLiteral("Echo");
  profile.command = QStringLiteral("node");
  profile.args = {QStringLiteral("server.js")};
  profile.env.insert(QStringLiteral("API_TOKEN"), QStringLiteral("secret"));

  const auto saved = store.upsert(profile);
  store.set_recent_profile_id(saved.id);

  REQUIRE_FALSE(saved.id.isEmpty());
  REQUIRE(store.profiles().size() == 1);
  REQUIRE(store.recent_profile().has_value());
  REQUIRE(store.recent_profile()->command == QStringLiteral("node"));

  const auto clone = store.clone_profile(saved.id);
  REQUIRE(clone.has_value());
  REQUIRE(clone->id != saved.id);
  REQUIRE(clone->name == QStringLiteral("Echo Copy"));
  REQUIRE(store.profiles().size() == 2);

  REQUIRE(store.remove(saved.id));
  REQUIRE_FALSE(store.remove(QStringLiteral("missing")));
}

TEST_CASE("ProfileStore persists profiles to SQLite", "[config][profile][sqlite]")
{
  if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE"))) {
    SUCCEED("QSQLITE driver is not available in this Qt build.");
    return;
  }

  QTemporaryDir temp_dir;
  REQUIRE(temp_dir.isValid());

  const auto database_path = QDir(temp_dir.path()).filePath(QStringLiteral("profiles.sqlite"));

  auto store = config::ProfileStore::sqlite(database_path);
  config::ServerProfile profile;
  profile.name = QStringLiteral("Persisted");
  profile.command = QStringLiteral("python");
  profile.args = {QStringLiteral("-m"), QStringLiteral("server")};
  profile.cwd = temp_dir.path();
  profile.timeout_ms = 2500;
  profile.env.insert(QStringLiteral("DEBUG"), QStringLiteral("1"));

  const auto saved = store.upsert(profile);
  store.set_recent_profile_id(saved.id);

  QString error_message;
  REQUIRE(store.save(&error_message));

  auto loaded = config::ProfileStore::sqlite(database_path);
  REQUIRE(loaded.load(&error_message));
  REQUIRE(loaded.profiles().size() == 1);
  REQUIRE(loaded.recent_profile_id() == saved.id);

  const auto restored = loaded.profile_by_id(saved.id);
  REQUIRE(restored.has_value());
  REQUIRE(restored->name == QStringLiteral("Persisted"));
  REQUIRE(restored->args == QStringList{QStringLiteral("-m"), QStringLiteral("server")});
  REQUIRE(restored->env.value(QStringLiteral("DEBUG")) == QStringLiteral("1"));
  REQUIRE(restored->timeout_ms == 2500);
}

TEST_CASE("ProfileStore persists streamable HTTP endpoint profiles to SQLite", "[config][profile][sqlite][http]")
{
  if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE"))) {
    SUCCEED("QSQLITE driver is not available in this Qt build.");
    return;
  }

  QTemporaryDir temp_dir;
  REQUIRE(temp_dir.isValid());

  const auto database_path = QDir(temp_dir.path()).filePath(QStringLiteral("profiles.sqlite"));

  auto store = config::ProfileStore::sqlite(database_path);
  config::ServerProfile profile;
  profile.name = QStringLiteral("HTTP MCP");
  profile.transport = config::ServerTransportKind::StreamableHttp;
  profile.endpoint_url = QStringLiteral("https://example.test/mcp");
  profile.protocol_version = QStringLiteral("2025-06-18");
  profile.http_headers.insert(QStringLiteral("Authorization"), QStringLiteral("Bearer token"));

  const auto saved = store.upsert(profile);
  store.set_recent_profile_id(saved.id);

  QString error_message;
  REQUIRE(store.save(&error_message));

  auto loaded = config::ProfileStore::sqlite(database_path);
  REQUIRE(loaded.load(&error_message));
  REQUIRE(loaded.profiles().size() == 1);

  const auto restored = loaded.profile_by_id(saved.id);
  REQUIRE(restored.has_value());
  REQUIRE(restored->transport == config::ServerTransportKind::StreamableHttp);
  REQUIRE(restored->endpoint_url == QStringLiteral("https://example.test/mcp"));
  REQUIRE(restored->protocol_version == QStringLiteral("2025-06-18"));
  REQUIRE(restored->http_headers.value(QStringLiteral("Authorization")) == QStringLiteral("Bearer token"));
}
