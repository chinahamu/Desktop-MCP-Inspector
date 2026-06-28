#include "RecorderReplay.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QUuid>

#include <utility>

namespace recorder {
namespace {
QString make_id() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
AssertionResult result_for(const Assertion& assertion, bool matched, QJsonValue actual, QString message)
{
  const auto passed = assertion.negated ? !matched : matched;
  return AssertionResult{assertion, passed, passed ? QStringLiteral("Passed: %1").arg(assertion.name) : std::move(message), std::move(actual)};
}
QString expect_msg(const QString& op, const QJsonValue& actual, const QJsonValue& expected)
{
  return QStringLiteral("Expected %1. actual=%2 expected=%3").arg(op, json_value_to_compact_string(actual), json_value_to_compact_string(expected));
}
} // namespace

QString assertion_kind_to_string(AssertionKind kind)
{
  switch (kind) {
  case AssertionKind::Equals: return QStringLiteral("equals");
  case AssertionKind::Contains: return QStringLiteral("contains");
  case AssertionKind::JsonPathEquals: return QStringLiteral("jsonpath_equals");
  case AssertionKind::JsonPathContains: return QStringLiteral("jsonpath_contains");
  case AssertionKind::JsonPathExists: return QStringLiteral("jsonpath_exists");
  }
  return QStringLiteral("equals");
}

std::optional<AssertionKind> assertion_kind_from_string(const QString& value)
{
  const auto text = value.trimmed().toLower();
  if (text == QStringLiteral("equals")) return AssertionKind::Equals;
  if (text == QStringLiteral("contains")) return AssertionKind::Contains;
  if (text == QStringLiteral("jsonpath_equals")) return AssertionKind::JsonPathEquals;
  if (text == QStringLiteral("jsonpath_contains")) return AssertionKind::JsonPathContains;
  if (text == QStringLiteral("jsonpath_exists")) return AssertionKind::JsonPathExists;
  return std::nullopt;
}

QString json_value_to_compact_string(const QJsonValue& value)
{
  if (value.isObject()) return QString::fromUtf8(QJsonDocument{value.toObject()}.toJson(QJsonDocument::Compact));
  if (value.isArray()) return QString::fromUtf8(QJsonDocument{value.toArray()}.toJson(QJsonDocument::Compact));
  if (value.isString()) return value.toString();
  if (value.isDouble()) return QString::number(value.toDouble(), 'g', 15);
  if (value.isBool()) return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
  if (value.isNull()) return QStringLiteral("null");
  return QStringLiteral("<undefined>");
}

bool json_value_contains(const QJsonValue& actual, const QJsonValue& expected)
{
  if (actual == expected) return true;
  if (actual.isString() && expected.isString()) return actual.toString().contains(expected.toString());
  if (actual.isObject() && expected.isObject()) {
    const auto object = actual.toObject();
    for (auto it = expected.toObject().constBegin(); it != expected.toObject().constEnd(); ++it) {
      if (!object.contains(it.key()) || !json_value_contains(object.value(it.key()), it.value())) return false;
    }
    return true;
  }
  if (actual.isArray()) {
    for (const auto& item : actual.toArray()) {
      if (json_value_contains(item, expected)) return true;
    }
  }
  return false;
}

std::optional<QJsonValue> resolve_json_path(const QJsonValue& root, const QString& path, QString* error_message)
{
  auto current = root;
  auto text = path.trimmed();
  if (text == QStringLiteral("$") || text.isEmpty()) return current;
  if (text.startsWith(QStringLiteral("$."))) text.remove(0, 2);
  for (const auto& raw_part : text.split(QStringLiteral("."), Qt::SkipEmptyParts)) {
    auto part = raw_part;
    int array_index = -1;
    const auto bracket = part.indexOf(QLatin1Char('['));
    if (bracket >= 0 && part.endsWith(QLatin1Char(']'))) {
      array_index = part.mid(bracket + 1, part.size() - bracket - 2).toInt();
      part = part.left(bracket);
    }
    if (!current.isObject() || !current.toObject().contains(part)) {
      if (error_message != nullptr) *error_message = QStringLiteral("JSONPath not found: %1").arg(path);
      return std::nullopt;
    }
    current = current.toObject().value(part);
    if (array_index >= 0) {
      if (!current.isArray() || array_index >= current.toArray().size()) return std::nullopt;
      current = current.toArray().at(array_index);
    }
  }
  return current;
}

AssertionResult evaluate_assertion(const Assertion& assertion, const QJsonObject& actual_result)
{
  const QJsonValue root{actual_result};
  if (assertion.kind == AssertionKind::Equals) return result_for(assertion, root == assertion.expected, root, expect_msg(QStringLiteral("root equals expected"), root, assertion.expected));
  if (assertion.kind == AssertionKind::Contains) return result_for(assertion, json_value_contains(root, assertion.expected), root, expect_msg(QStringLiteral("root contains expected"), root, assertion.expected));
  QString error;
  const auto actual = resolve_json_path(root, assertion.json_path, &error);
  if (!actual.has_value()) return result_for(assertion, false, {}, error);
  if (assertion.kind == AssertionKind::JsonPathExists) return result_for(assertion, true, *actual, QStringLiteral("JSONPath exists"));
  const auto matched = assertion.kind == AssertionKind::JsonPathContains ? json_value_contains(*actual, assertion.expected) : *actual == assertion.expected;
  return result_for(assertion, matched, *actual, expect_msg(assertion.json_path, *actual, assertion.expected));
}

Assertion equals_assertion(QString name, QJsonValue expected) { return {make_id(), std::move(name), AssertionKind::Equals, {}, std::move(expected), false}; }
Assertion contains_assertion(QString name, QJsonValue expected) { return {make_id(), std::move(name), AssertionKind::Contains, {}, std::move(expected), false}; }
Assertion jsonpath_equals_assertion(QString name, QString path, QJsonValue expected) { return {make_id(), std::move(name), AssertionKind::JsonPathEquals, std::move(path), std::move(expected), false}; }
Assertion jsonpath_contains_assertion(QString name, QString path, QJsonValue expected) { return {make_id(), std::move(name), AssertionKind::JsonPathContains, std::move(path), std::move(expected), false}; }
Assertion jsonpath_exists_assertion(QString name, QString path) { return {make_id(), std::move(name), AssertionKind::JsonPathExists, std::move(path), {}, false}; }

QJsonObject to_json_object(const Assertion& assertion)
{
  QJsonObject object{{QStringLiteral("id"), assertion.id}, {QStringLiteral("name"), assertion.name}, {QStringLiteral("kind"), assertion_kind_to_string(assertion.kind)}};
  if (!assertion.json_path.isEmpty()) object.insert(QStringLiteral("jsonPath"), assertion.json_path);
  if (!assertion.expected.isUndefined()) object.insert(QStringLiteral("expected"), assertion.expected);
  if (assertion.negated) object.insert(QStringLiteral("negated"), true);
  return object;
}

std::optional<Assertion> assertion_from_json_object(const QJsonObject& object, QString* error_message)
{
  const auto kind = assertion_kind_from_string(object.value(QStringLiteral("kind")).toString());
  if (!kind.has_value()) { if (error_message != nullptr) *error_message = QStringLiteral("Unknown assertion kind."); return std::nullopt; }
  Assertion assertion;
  assertion.id = object.value(QStringLiteral("id")).toString(make_id());
  assertion.name = object.value(QStringLiteral("name")).toString(assertion_kind_to_string(*kind));
  assertion.kind = *kind;
  assertion.json_path = object.value(QStringLiteral("jsonPath")).toString();
  assertion.expected = object.value(QStringLiteral("expected"));
  assertion.negated = object.value(QStringLiteral("negated")).toBool(false);
  return assertion;
}

} // namespace recorder
