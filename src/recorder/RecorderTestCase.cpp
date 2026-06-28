#include "RecorderReplay.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QUuid>

#include <utility>

namespace recorder {
namespace {
QString make_id() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
QDateTime now_utc() { return QDateTime::currentDateTimeUtc(); }
QJsonArray assertions_to_json(const QVector<Assertion>& assertions)
{
  QJsonArray array;
  for (const auto& assertion : assertions) array.append(to_json_object(assertion));
  return array;
}
} // namespace

TestCase make_test_case_from_tool_call(QString tool_name, QJsonObject arguments, QJsonObject result, QString profile_id, QString name)
{
  const auto timestamp = now_utc();
  if (name.trimmed().isEmpty()) name = QStringLiteral("%1 @ %2").arg(tool_name, timestamp.toLocalTime().toString(Qt::ISODate));
  TestCase test_case;
  test_case.id = make_id();
  test_case.name = std::move(name);
  test_case.profile_id = std::move(profile_id);
  test_case.tool_name = std::move(tool_name);
  test_case.arguments = std::move(arguments);
  test_case.expected_result = std::move(result);
  test_case.assertions.push_back(equals_assertion(QStringLiteral("result equals recorded output"), test_case.expected_result));
  test_case.created_at = timestamp;
  test_case.updated_at = timestamp;
  return test_case;
}

bool is_valid_test_case(const TestCase& test_case, QString* error_message)
{
  if (test_case.id.trimmed().isEmpty()) { if (error_message != nullptr) *error_message = QStringLiteral("TestCase id is required."); return false; }
  if (test_case.name.trimmed().isEmpty()) { if (error_message != nullptr) *error_message = QStringLiteral("TestCase name is required."); return false; }
  if (test_case.tool_name.trimmed().isEmpty()) { if (error_message != nullptr) *error_message = QStringLiteral("TestCase toolName is required."); return false; }
  if (test_case.assertions.empty()) { if (error_message != nullptr) *error_message = QStringLiteral("TestCase must have assertions."); return false; }
  return true;
}

QJsonObject to_json_object(const TestCase& test_case)
{
  QJsonObject object{
      {QStringLiteral("id"), test_case.id},
      {QStringLiteral("name"), test_case.name},
      {QStringLiteral("profileId"), test_case.profile_id},
      {QStringLiteral("toolName"), test_case.tool_name},
      {QStringLiteral("arguments"), test_case.arguments},
      {QStringLiteral("expectedResult"), test_case.expected_result},
      {QStringLiteral("assertions"), assertions_to_json(test_case.assertions)},
      {QStringLiteral("createdAt"), test_case.created_at.toUTC().toString(Qt::ISODateWithMs)},
      {QStringLiteral("updatedAt"), test_case.updated_at.toUTC().toString(Qt::ISODateWithMs)},
  };
  return object;
}

std::optional<TestCase> test_case_from_json_object(const QJsonObject& object, QString* error_message)
{
  if (!object.value(QStringLiteral("arguments")).isObject() || !object.value(QStringLiteral("expectedResult")).isObject()) {
    if (error_message != nullptr) *error_message = QStringLiteral("TestCase arguments and expectedResult must be objects.");
    return std::nullopt;
  }
  TestCase test_case;
  test_case.id = object.value(QStringLiteral("id")).toString(make_id());
  test_case.name = object.value(QStringLiteral("name")).toString();
  test_case.profile_id = object.value(QStringLiteral("profileId")).toString();
  test_case.tool_name = object.value(QStringLiteral("toolName")).toString();
  test_case.arguments = object.value(QStringLiteral("arguments")).toObject();
  test_case.expected_result = object.value(QStringLiteral("expectedResult")).toObject();
  test_case.created_at = QDateTime::fromString(object.value(QStringLiteral("createdAt")).toString(), Qt::ISODateWithMs);
  test_case.updated_at = QDateTime::fromString(object.value(QStringLiteral("updatedAt")).toString(), Qt::ISODateWithMs);
  if (!test_case.created_at.isValid()) test_case.created_at = now_utc();
  if (!test_case.updated_at.isValid()) test_case.updated_at = test_case.created_at;
  if (test_case.name.trimmed().isEmpty()) test_case.name = test_case.tool_name;
  test_case.assertions.push_back(equals_assertion(QStringLiteral("result equals recorded output"), test_case.expected_result));
  if (!is_valid_test_case(test_case, error_message)) return std::nullopt;
  return test_case;
}

QByteArray export_test_cases_json(const QVector<TestCase>& test_cases)
{
  QJsonArray array;
  for (const auto& test_case : test_cases) array.append(to_json_object(test_case));
  return QJsonDocument{QJsonObject{{QStringLiteral("version"), 1}, {QStringLiteral("testCases"), array}}}.toJson(QJsonDocument::Indented);
}

ImportResult import_test_cases_json(const QByteArray& payload)
{
  ImportResult result;
  QJsonParseError parse_error;
  const auto document = QJsonDocument::fromJson(payload, &parse_error);
  if (parse_error.error != QJsonParseError::NoError) { result.error_message = parse_error.errorString(); return result; }
  const auto array = document.isArray() ? document.array() : document.object().value(QStringLiteral("testCases")).toArray();
  for (const auto& value : array) {
    auto test_case = value.isObject() ? test_case_from_json_object(value.toObject(), &result.error_message) : std::optional<TestCase>{};
    if (!test_case.has_value()) { result.test_cases.clear(); return result; }
    result.test_cases.push_back(std::move(*test_case));
  }
  return result;
}

ImportResult import_test_cases_json(const QString& payload) { return import_test_cases_json(payload.toUtf8()); }

} // namespace recorder
