#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>
#include <optional>

namespace recorder {

enum class AssertionKind { Equals, Contains, JsonPathEquals, JsonPathContains, JsonPathExists };

struct Assertion {
  QString id;
  QString name;
  AssertionKind kind = AssertionKind::Equals;
  QString json_path;
  QJsonValue expected;
  bool negated = false;
};

struct TestCase {
  QString id;
  QString name;
  QString profile_id;
  QString tool_name;
  QJsonObject arguments;
  QJsonObject expected_result;
  QVector<Assertion> assertions;
  QDateTime created_at;
  QDateTime updated_at;
};

struct AssertionResult {
  Assertion assertion;
  bool passed = false;
  QString message;
  QJsonValue actual;
};

struct ReplayInvocationResult {
  bool ok = true;
  QJsonObject result;
  QString error_message;
};

struct ReplayCaseResult {
  TestCase test_case;
  bool invocation_ok = false;
  QString invocation_message;
  QJsonObject actual_result;
  QVector<AssertionResult> assertions;
  [[nodiscard]] bool passed() const;
};

struct ReplayRunResult {
  QVector<ReplayCaseResult> cases;
  [[nodiscard]] int passed_count() const;
  [[nodiscard]] int failed_count() const;
  [[nodiscard]] bool passed() const;
  [[nodiscard]] QString summary() const;
};

using ReplayToolInvoker = std::function<ReplayInvocationResult(const TestCase&)>;

[[nodiscard]] QString assertion_kind_to_string(AssertionKind kind);
[[nodiscard]] std::optional<AssertionKind> assertion_kind_from_string(const QString& value);
[[nodiscard]] QString json_value_to_compact_string(const QJsonValue& value);
[[nodiscard]] bool json_value_contains(const QJsonValue& actual, const QJsonValue& expected);
[[nodiscard]] std::optional<QJsonValue> resolve_json_path(const QJsonValue& root, const QString& path, QString* error_message = nullptr);
[[nodiscard]] AssertionResult evaluate_assertion(const Assertion& assertion, const QJsonObject& actual_result);

[[nodiscard]] Assertion equals_assertion(QString name, QJsonValue expected);
[[nodiscard]] Assertion contains_assertion(QString name, QJsonValue expected);
[[nodiscard]] Assertion jsonpath_equals_assertion(QString name, QString path, QJsonValue expected);
[[nodiscard]] Assertion jsonpath_contains_assertion(QString name, QString path, QJsonValue expected);
[[nodiscard]] Assertion jsonpath_exists_assertion(QString name, QString path);

[[nodiscard]] TestCase make_test_case_from_tool_call(
    QString tool_name,
    QJsonObject arguments,
    QJsonObject result,
    QString profile_id = {},
    QString name = {});
[[nodiscard]] bool is_valid_test_case(const TestCase& test_case, QString* error_message = nullptr);

[[nodiscard]] QJsonObject to_json_object(const Assertion& assertion);
[[nodiscard]] std::optional<Assertion> assertion_from_json_object(const QJsonObject& object, QString* error_message = nullptr);
[[nodiscard]] QJsonObject to_json_object(const TestCase& test_case);
[[nodiscard]] std::optional<TestCase> test_case_from_json_object(const QJsonObject& object, QString* error_message = nullptr);

[[nodiscard]] QByteArray export_test_cases_json(const QVector<TestCase>& test_cases);
struct ImportResult { QVector<TestCase> test_cases; QString error_message; [[nodiscard]] bool ok() const { return error_message.isEmpty(); } };
[[nodiscard]] ImportResult import_test_cases_json(const QByteArray& payload);
[[nodiscard]] ImportResult import_test_cases_json(const QString& payload);

class TestCaseStore final {
public:
  TestCaseStore();
  explicit TestCaseStore(QString database_path);
  [[nodiscard]] static QString default_sqlite_path();
  [[nodiscard]] QVector<TestCase> load(QString* error_message = nullptr) const;
  bool upsert(TestCase test_case, QString* error_message = nullptr) const;
  bool remove(const QString& id, QString* error_message = nullptr) const;
  [[nodiscard]] const QString& database_path() const { return database_path_; }
private:
  QString database_path_;
};

class ReplayRunner final {
public:
  explicit ReplayRunner(ReplayToolInvoker invoker = {});
  [[nodiscard]] ReplayCaseResult run_one(const TestCase& test_case) const;
  [[nodiscard]] ReplayRunResult run(const QVector<TestCase>& test_cases, bool stop_on_failure = false) const;
private:
  ReplayToolInvoker invoker_;
};

[[nodiscard]] ReplayToolInvoker recorded_result_invoker();
[[nodiscard]] std::optional<int> maybe_run_replay_cli(const QStringList& arguments);

} // namespace recorder
