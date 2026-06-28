#include <catch2/catch_test_macros.hpp>

#include "ServerProfile.hpp"
#include "StdioTransport.hpp"
#include "Transport.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonObject>
#include <QJsonValue>
#include <QThread>

#include <algorithm>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

namespace {

QCoreApplication& ensure_qcore_application()
{
  if (auto* instance = QCoreApplication::instance()) {
    return *instance;
  }

  static int argc = 1;
  static char app_name[] = "desktop_mcp_inspector_tests";
  static char* argv[] = {app_name, nullptr};
  static QCoreApplication application(argc, argv);
  return application;
}

bool wait_until(const std::function<bool()>& predicate, int timeout_ms = 3000)
{
  auto& app = ensure_qcore_application();
  QElapsedTimer timer;
  timer.start();

  while (timer.elapsed() < timeout_ms) {
    if (predicate()) {
      return true;
    }

    app.processEvents(QEventLoop::AllEvents, 20);
    QThread::msleep(5);
  }

  app.processEvents(QEventLoop::AllEvents, 20);
  return predicate();
}

config::ServerProfile mock_server_profile(QStringList args)
{
  config::ServerProfile profile;
  profile.id = QStringLiteral("stdio-mock-server");
  profile.name = QStringLiteral("stdio mock server");
  profile.transport = config::ServerTransportKind::Stdio;
  profile.command = QString::fromUtf8(DMI_STDIO_MOCK_SERVER_PATH);
  profile.args = std::move(args);
  profile.timeout_ms = 3000;
  return profile;
}

} // namespace

TEST_CASE("stdio transport exchanges JSON-RPC messages with a mock server", "[transport][stdio][integration]")
{
  ensure_qcore_application();

  transport::StdioTransport subject;
  std::vector<QJsonObject> messages;
  std::vector<QString> stderr_lines;
  std::vector<transport::TransportError> errors;

  REQUIRE(QObject::connect(
      &subject,
      &transport::Transport::messageReceived,
      [&messages](const QJsonObject& message) { messages.push_back(message); }));
  REQUIRE(QObject::connect(
      &subject,
      &transport::Transport::stderrReceived,
      [&stderr_lines](const QString& line) { stderr_lines.push_back(line); }));
  REQUIRE(QObject::connect(
      &subject,
      &transport::Transport::errorOccurred,
      [&errors](const transport::TransportError& error) { errors.push_back(error); }));

  subject.start(mock_server_profile(QStringList{QStringLiteral("--echo-once")}));
  REQUIRE(subject.state() == transport::TransportState::Running);

  subject.send(QJsonObject{
      {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
      {QStringLiteral("id"), 1},
      {QStringLiteral("method"), QStringLiteral("tools/list")},
  });

  REQUIRE(wait_until([&messages] { return !messages.empty(); }));
  REQUIRE(messages.size() == 1);
  REQUIRE(messages.front().value(QStringLiteral("jsonrpc")).toString() == QStringLiteral("2.0"));
  REQUIRE(messages.front().value(QStringLiteral("id")).toInt() == 1);
  REQUIRE(messages.front().value(QStringLiteral("result")).toObject().value(QStringLiteral("mode")).toString()
          == QStringLiteral("echo-once"));

  REQUIRE(wait_until([&stderr_lines] { return !stderr_lines.empty(); }));
  REQUIRE(stderr_lines.front() == QStringLiteral("mock stderr ready"));
  REQUIRE(std::none_of(errors.cbegin(), errors.cend(), [](const transport::TransportError& error) {
    return error.category == transport::TransportErrorCategory::ProtocolViolation;
  }));

  subject.stop();
  REQUIRE(subject.state() == transport::TransportState::Stopped);
}

TEST_CASE("stdio transport reports protocol violations from a mock server", "[transport][stdio][integration]")
{
  ensure_qcore_application();

  transport::StdioTransport subject;
  std::vector<QJsonObject> messages;
  std::vector<transport::TransportError> errors;

  REQUIRE(QObject::connect(
      &subject,
      &transport::Transport::messageReceived,
      [&messages](const QJsonObject& message) { messages.push_back(message); }));
  REQUIRE(QObject::connect(
      &subject,
      &transport::Transport::errorOccurred,
      [&errors](const transport::TransportError& error) { errors.push_back(error); }));

  subject.start(mock_server_profile(QStringList{QStringLiteral("--protocol-violation")}));
  REQUIRE(subject.state() == transport::TransportState::Running);

  REQUIRE(wait_until([&errors] {
    return std::any_of(errors.cbegin(), errors.cend(), [](const transport::TransportError& error) {
      return error.category == transport::TransportErrorCategory::ProtocolViolation;
    });
  }));

  REQUIRE(messages.empty());
  const auto violation = std::find_if(errors.cbegin(), errors.cend(), [](const transport::TransportError& error) {
    return error.category == transport::TransportErrorCategory::ProtocolViolation;
  });
  REQUIRE(violation != errors.cend());
  REQUIRE(violation->details.has_value());
  REQUIRE(violation->details->value(QStringLiteral("code")).toString() == QStringLiteral("invalid_version"));

  subject.stop();
}

TEST_CASE("stdio transport flushes stderr tail and reports unexpected mock server exit", "[transport][stdio][integration]")
{
  ensure_qcore_application();

  transport::StdioTransport subject;
  std::vector<QString> stderr_lines;
  std::vector<transport::TransportError> errors;

  REQUIRE(QObject::connect(
      &subject,
      &transport::Transport::stderrReceived,
      [&stderr_lines](const QString& line) { stderr_lines.push_back(line); }));
  REQUIRE(QObject::connect(
      &subject,
      &transport::Transport::errorOccurred,
      [&errors](const transport::TransportError& error) { errors.push_back(error); }));

  subject.start(mock_server_profile(QStringList{QStringLiteral("--stderr-tail-and-exit")}));

  REQUIRE(wait_until([&subject, &errors] {
    return subject.state() == transport::TransportState::Failed
        && std::any_of(errors.cbegin(), errors.cend(), [](const transport::TransportError& error) {
             return error.category == transport::TransportErrorCategory::ProcessExited;
           });
  }));

  REQUIRE(wait_until([&stderr_lines] { return !stderr_lines.empty(); }));
  REQUIRE(stderr_lines.front() == QStringLiteral("unterminated stderr tail"));

  const auto exit_error = std::find_if(errors.cbegin(), errors.cend(), [](const transport::TransportError& error) {
    return error.category == transport::TransportErrorCategory::ProcessExited;
  });
  REQUIRE(exit_error != errors.cend());
  REQUIRE(exit_error->exit_code.has_value());
  REQUIRE(*exit_error->exit_code == 7);
}
