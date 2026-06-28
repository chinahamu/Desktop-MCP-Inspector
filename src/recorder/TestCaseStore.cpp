#include "RecorderReplay.hpp"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>

#include <utility>

namespace recorder {
namespace {
QString connection_name() { return QStringLiteral("dmi_recorder_%1").arg(QUuid::createUuid().toString(QUuid::Id128)); }
bool open_db(const QString& path, const QString& name, QSqlDatabase* db, QString* error)
{
  const QFileInfo info(path);
  if (!info.absolutePath().isEmpty()) QDir().mkpath(info.absolutePath());
  *db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
  db->setDatabaseName(path);
  if (!db->open()) { if (error != nullptr) *error = db->lastError().text(); return false; }
  QSqlQuery query(*db);
  if (!query.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS test_cases (id TEXT PRIMARY KEY NOT NULL, payload_json TEXT NOT NULL, updated_at TEXT NOT NULL)"))) {
    if (error != nullptr) *error = query.lastError().text(); return false;
  }
  return true;
}
void close_db(const QString& name, QSqlDatabase* db) { db->close(); *db = QSqlDatabase(); QSqlDatabase::removeDatabase(name); }
} // namespace

TestCaseStore::TestCaseStore() = default;
TestCaseStore::TestCaseStore(QString database_path) : database_path_(std::move(database_path)) {}

QString TestCaseStore::default_sqlite_path()
{
  auto base = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
  if (base.isEmpty()) base = QDir::homePath() + QStringLiteral("/.DesktopMCPInspector");
  return QDir(base).filePath(QStringLiteral("test_cases.sqlite"));
}

QVector<TestCase> TestCaseStore::load(QString* error_message) const
{
  QVector<TestCase> cases;
  const auto name = connection_name();
  QSqlDatabase db;
  if (!open_db(database_path_, name, &db, error_message)) { close_db(name, &db); return cases; }
  QSqlQuery query(db);
  if (!query.exec(QStringLiteral("SELECT payload_json FROM test_cases ORDER BY updated_at DESC"))) {
    if (error_message != nullptr) *error_message = query.lastError().text();
    close_db(name, &db); return cases;
  }
  while (query.next()) {
    const auto doc = QJsonDocument::fromJson(query.value(0).toString().toUtf8());
    auto test_case = doc.isObject() ? test_case_from_json_object(doc.object(), error_message) : std::optional<TestCase>{};
    if (test_case.has_value()) cases.push_back(std::move(*test_case));
  }
  close_db(name, &db);
  return cases;
}

bool TestCaseStore::upsert(TestCase test_case, QString* error_message) const
{
  if (!is_valid_test_case(test_case, error_message)) return false;
  if (!test_case.created_at.isValid()) test_case.created_at = QDateTime::currentDateTimeUtc();
  test_case.updated_at = QDateTime::currentDateTimeUtc();
  const auto name = connection_name();
  QSqlDatabase db;
  if (!open_db(database_path_, name, &db, error_message)) { close_db(name, &db); return false; }
  QSqlQuery query(db);
  query.prepare(QStringLiteral("INSERT OR REPLACE INTO test_cases (id,payload_json,updated_at) VALUES (?,?,?)"));
  query.addBindValue(test_case.id);
  query.addBindValue(QString::fromUtf8(QJsonDocument{to_json_object(test_case)}.toJson(QJsonDocument::Compact)));
  query.addBindValue(test_case.updated_at.toUTC().toString(Qt::ISODateWithMs));
  const auto ok = query.exec();
  if (!ok && error_message != nullptr) *error_message = query.lastError().text();
  close_db(name, &db);
  return ok;
}

bool TestCaseStore::remove(const QString& id, QString* error_message) const
{
  const auto name = connection_name();
  QSqlDatabase db;
  if (!open_db(database_path_, name, &db, error_message)) { close_db(name, &db); return false; }
  QSqlQuery query(db);
  query.prepare(QStringLiteral("DELETE FROM test_cases WHERE id=?"));
  query.addBindValue(id);
  const auto ok = query.exec();
  if (!ok && error_message != nullptr) *error_message = query.lastError().text();
  close_db(name, &db);
  return ok;
}

} // namespace recorder
