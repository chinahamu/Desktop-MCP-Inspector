#include <catch2/catch_test_macros.hpp>

#include "Transport.hpp"

#include <QJsonObject>
#include <QObject>
#include <QString>

#include <optional>
#include <utility>

namespace {

class RecordingTransport final : public transport::Transport
{
public:
  using transport::Transport::Transport;

  void start(const config::ServerProfile&) override
  {
    update_state(transport::TransportState::Starting);
  }

  void stop() override
  {
    update_state(transport::TransportState::Stopped);
  }

  void send(const QJsonObject& message) override
  {
    last_sent_message = message;
  }

  [[nodiscard]] transport::TransportState state() const override
  {
    return current_state;
  }

  void update_state(transport::TransportState next_state)
  {
    current_state = next_state;
    emit stateChanged(current_state);
  }

  void publish_message(const QJsonObject& message)
  {
    emit messageReceived(message);
  }

  void publish_stderr(QString text)
  {
    emit stderrReceived(std::move(text));
  }

  void publish_error(const transport::TransportError& error)
  {
    emit errorOccurred(error);
  }

  transport::TransportState current_state = transport::TransportState::Idle;
  QJsonObject last_sent_message;
};

} // namespace

TEST_CASE("transport interface exposes lifecycle state helpers", "[transport]")
{
  RecordingTransport subject;

  REQUIRE(subject.state() == transport::TransportState::Idle);
  REQUIRE_FALSE(subject.is_running());
  REQUIRE(transport::transport_state_name(transport::TransportState::Idle) == QStringLiteral("idle"));
  REQUIRE(transport::transport_state_name(transport::TransportState::Starting) == QStringLiteral("starting"));
  REQUIRE(transport::transport_state_name(transport::TransportState::Running) == QStringLiteral("running"));
  REQUIRE(transport::transport_state_name(transport::TransportState::Stopping) == QStringLiteral("stopping"));
  REQUIRE(transport::transport_state_name(transport::TransportState::Stopped) == QStringLiteral("stopped"));
  REQUIRE(transport::transport_state_name(transport::TransportState::Failed) == QStringLiteral("failed"));

  subject.update_state(transport::TransportState::Running);

  REQUIRE(subject.state() == transport::TransportState::Running);
  REQUIRE(subject.is_running());
}

TEST_CASE("transport interface publishes Qt signals for state and streams", "[transport]")
{
  RecordingTransport subject;

  int state_signal_count = 0;
  auto last_state = transport::TransportState::Idle;
  const auto state_connection = QObject::connect(
      &subject,
      &transport::Transport::stateChanged,
      [&state_signal_count, &last_state](transport::TransportState state) {
        ++state_signal_count;
        last_state = state;
      });
  REQUIRE(state_connection);

  QJsonObject received_message;
  const auto message_connection = QObject::connect(
      &subject,
      &transport::Transport::messageReceived,
      [&received_message](const QJsonObject& message) { received_message = message; });
  REQUIRE(message_connection);

  QString stderr_text;
  const auto stderr_connection = QObject::connect(
      &subject,
      &transport::Transport::stderrReceived,
      [&stderr_text](const QString& text) { stderr_text = text; });
  REQUIRE(stderr_connection);

  subject.stop();
  const QJsonObject message{{QStringLiteral("jsonrpc"), QStringLiteral("2.0")}};
  subject.publish_message(message);
  subject.publish_stderr(QStringLiteral("server warning"));

  REQUIRE(state_signal_count == 1);
  REQUIRE(last_state == transport::TransportState::Stopped);
  REQUIRE(received_message == message);
  REQUIRE(stderr_text == QStringLiteral("server warning"));
}

TEST_CASE("transport interface keeps send payloads and structured errors", "[transport]")
{
  RecordingTransport subject;

  const QJsonObject request{
      {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
      {QStringLiteral("id"), 1},
      {QStringLiteral("method"), QStringLiteral("initialize")},
  };

  subject.send(request);

  REQUIRE(subject.last_sent_message == request);

  transport::TransportError expected_error;
  expected_error.category = transport::TransportErrorCategory::ProtocolViolation;
  expected_error.message = QStringLiteral("stdout contained non JSON-RPC data");
  expected_error.details = QJsonObject{{QStringLiteral("raw"), QStringLiteral("not json")}};

  std::optional<transport::TransportError> captured_error;
  const auto error_connection = QObject::connect(
      &subject,
      &transport::Transport::errorOccurred,
      [&captured_error](const transport::TransportError& error) { captured_error = error; });
  REQUIRE(error_connection);

  subject.publish_error(expected_error);

  REQUIRE(captured_error.has_value());
  REQUIRE(*captured_error == expected_error);
  REQUIRE(
      transport::transport_error_category_name(transport::TransportErrorCategory::ProtocolViolation)
      == QStringLiteral("protocol_violation"));
  REQUIRE(
      transport::transport_error_category_name(transport::TransportErrorCategory::StartFailed)
      == QStringLiteral("start_failed"));
}
