#include "RecorderReplay.hpp"

#include <utility>

namespace recorder {

bool ReplayCaseResult::passed() const
{
  if (!invocation_ok || assertions.empty()) return false;
  for (const auto& assertion : assertions) {
    if (!assertion.passed) return false;
  }
  return true;
}

int ReplayRunResult::passed_count() const
{
  int count = 0;
  for (const auto& result : cases) {
    if (result.passed()) ++count;
  }
  return count;
}

int ReplayRunResult::failed_count() const
{
  return cases.size() - passed_count();
}

bool ReplayRunResult::passed() const
{
  return !cases.empty() && failed_count() == 0;
}

QString ReplayRunResult::summary() const
{
  return QStringLiteral("%1/%2 replay cases passed").arg(passed_count()).arg(cases.size());
}

ReplayRunner::ReplayRunner(ReplayToolInvoker invoker) : invoker_(std::move(invoker))
{
  if (!invoker_) invoker_ = recorded_result_invoker();
}

ReplayCaseResult ReplayRunner::run_one(const TestCase& test_case) const
{
  ReplayCaseResult result;
  result.test_case = test_case;
  const auto invocation = invoker_(test_case);
  result.invocation_ok = invocation.ok;
  result.invocation_message = invocation.ok ? QStringLiteral("Invocation succeeded") : invocation.error_message;
  result.actual_result = invocation.result;
  if (!invocation.ok) return result;
  for (const auto& assertion : test_case.assertions) {
    result.assertions.push_back(evaluate_assertion(assertion, invocation.result));
  }
  return result;
}

ReplayRunResult ReplayRunner::run(const QVector<TestCase>& test_cases, bool stop_on_failure) const
{
  ReplayRunResult result;
  for (const auto& test_case : test_cases) {
    auto case_result = run_one(test_case);
    const auto ok = case_result.passed();
    result.cases.push_back(std::move(case_result));
    if (!ok && stop_on_failure) break;
  }
  return result;
}

ReplayToolInvoker recorded_result_invoker()
{
  return [](const TestCase& test_case) {
    return ReplayInvocationResult{true, test_case.expected_result, {}};
  };
}

std::optional<int> maybe_run_replay_cli(const QStringList& arguments)
{
  for (const auto& argument : arguments) {
    if (argument == QStringLiteral("--replay") || argument.startsWith(QStringLiteral("--replay=")) || argument == QStringLiteral("replay")) {
      return 2;
    }
  }
  return std::nullopt;
}

} // namespace recorder
