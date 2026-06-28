#include <catch2/catch_test_macros.hpp>

#include "ServerProfile.hpp"
#include "TransportConnection.hpp"

#include <QJsonObject>
#include <QObject>
#include <QString>

#include <memory>
#include <optional>
#include <vector>

namespace {

class FakeTransport final : public transport::Transport
{
public:
  explicit FakeTransport(QObject* parent = nullptr)
    : Transport(parent)
  {
  }

  void start(const config::ServerProfile& profile) override
  {
    ++start_calls;
    started_profiles.push_back(profile);
    set_state(next_start_state);
  }

  void stop() override
  {
    ++stop_calls;
    set_state(next_stop_state);
  }

  void send(const QJsonObject& message) override
  {
    ++send_calls;
    sent_messages.push_back(message);
  }

  [[nodiscard]] transport::TransportState state() const override
  {
    return state_;
  }

  void set_state(transport::TransportState next_state)
  {
    state_ = next_state;
    emit stateChanged(state_);
  }

  void emit_message(const QJsonObject& message)
  {
    emit messageReceived(message);
  }

  void emit_stderr(const QString& line)
  {
    emit stderrReceived(line);
  }

  void emit_error(const transport::TransportError& error)
  {
    emit errorOccurred(error);
  }

  int start_calls = 0;
  int stop_calls = 0;
  int send_calls = 0;
  transport::TransportState next_start_state = transport::TransportState::Running;
  transport::TransportState next_stop_state = transport::TransportState::Stopped;
  std::vector<config::ServerProfile> started_profiles;
  std::vector<QJsonObject> sent_messages;

private:
  transport::TransportState state_ = transport::TransportState::Idle;
};

config::ServerProfile make_profile(QString id = QStringLiteral("profile-1"))
{
  config::ServerProfile profile;
  profile.id = id;
  profile.name = QStringLiteral("Test Profile");
  profile.transport = config::ServerTransportKind::Stdio;
  profile.command = QStringLiteral("mock-server");
  profile.args = QStringList{QStringLiteral("--serve")};
  profile.timeout_ms = 1000;
  return profile;
}

} // namespace

TEST_CASE("transport connection connects and stores active profile", "[transport][connection]")
{
  auto fake_transport = std::make_unique<FakeTransport>();
  auto* raw_transport = fake_transport.get();
  transport::TransportConnection connection(std::move(fake_transport));

  std::vector<transport::TransportState> states;
  REQUIRE(QObject::connect(
      &connection,
      &transport::TransportConnection::stateChanged,
      [&states](transport::TransportState state) { states.push_back(state); }));

  const auto profile = make_profile();

  REQUIRE(connection.connect_to(profile));

  REQUIRE(raw_transport->start_calls == 1);
  REQUIRE(raw_transport->started_profiles.size() == 1);
  REQUIRE(raw_transport->started_profiles.front() == profile);
  REQUIRE(connection.is_connected());
  REQUIRE(connection.has_profile());
  REQUIRE(connection.profile().has_value());
  REQUIRE(*connection.profile() == profile);
  REQUIRE(states.size() == 1);
  REQUIRE(states.front() == transport::TransportState::Running);
}

TEST_CASE("transport connection disconnects an active transport", "[transport][connection]")
{
  auto fake_transport = std::make_unique<FakeTransport>();
  auto* raw_transport = fake_transport.get();
  transport::TransportConnection connection(std::move(fake_transport));

  REQUIRE(connection.connect_to(make_profile()));
  REQUIRE(connection.disconnect());

  REQUIRE(raw_transport->stop_calls == 1);
  REQUIRE(connection.state() == transport::TransportState::Stopped);
  REQUIRE_FALSE(connection.is_connected());
  REQUIRE(connection.has_profile());
}

TEST_CASE("transport connection does not disconnect an idle transport", "[transport][connection]")
{
  auto fake_transport = std::make_unique<FakeTransport>();
  auto* raw_transport = fake_transport.get();
  transport::TransportConnection connection(std::move(fake_transport));

  REQUIRE_FALSE(connection.disconnect());
  REQUIRE(raw_transport->stop_calls == 0);
}

TEST_CASE("transport connection reconnects using the last profile", "[transport][connection]")
{
  auto fake_transport = std::make_unique<FakeTransport>();
  auto* raw_transport = fake_transport.get();
  transport::TransportConnection connection(std::move(fake_transport));

  const auto profile = make_profile(QStringLiteral("reconnect-profile"));
  REQUIRE(connection.connect_to(profile));
  REQUIRE(connection.reconnect());

  REQUIRE(raw_transport->stop_calls == 1);
  REQUIRE(raw_transport->start_calls == 2);
  REQUIRE(raw_transport->started_profiles.at(1) == profile);
  REQUIRE(connection.is_connected());
}

TEST_CASE("transport connection rejects reconnect before a profile is known", "[transport][connection]")
{
  auto fake_transport = std::make_unique<FakeTransport>();
  auto* raw_transport = fake_transport.get();
  transport::TransportConnection connection(std::move(fake_transport));

  REQUIRE_FALSE(connection.reconnect());
  REQUIRE(raw_transport->start_calls == 0);
  REQUIRE(raw_transport->stop_calls == 0);
}

TEST_CASE("transport connection forwards send calls", "[transport][connection]")
{
  auto fake_transport = std::make_unique<FakeTransport>();
  auto* raw_transport = fake_transport.get();
  transport::TransportConnection connection(std::move(fake_transport));

  const QJsonObject message{
      {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
      {QStringLiteral("method"), QStringLiteral("tools/list")},
  };

  connection.send(message);

  REQUIRE(raw_transport->send_calls == 1);
  REQUIRE(raw_transport->sent_messages.size() == 1);
  REQUIRE(raw_transport->sent_messages.front() == message);
}

TEST_CASE("transport connection forwards transport signals", "[transport][connection]")
{
  auto fake_transport = std::make_unique<FakeTransport>();
  auto* raw_transport = fake_transport.get();
  transport::TransportConnection connection(std::move(fake_transport));

  std::optional<QJsonObject> captured_message;
  std::optional<QString> captured_stderr;
  std::optional<transport::TransportError> captured_error;

  REQUIRE(QObject::connect(
      &connection,
      &transport::TransportConnection::messageReceived,
      [&captured_message](const QJsonObject& message) { captured_message = message; }));
  REQUIRE(QObject::connect(
      &connection,
      &transport::TransportConnection::stderrReceived,
      [&captured_stderr](const QString& line) { captured_stderr = line; }));
  REQUIRE(QObject::connect(
      &connection,
      &transport::TransportConnection::errorOccurred,
      [&captured_error](const transport::TransportError& error) { captured_error = error; }));

  const QJsonObject message{{QStringLiteral("ok"), true}};
  transport::TransportError error;
  error.category = transport::TransportErrorCategory::ProtocolViolation;
  error.message = QStringLiteral("bad stdout");

  raw_transport->emit_message(message);
  raw_transport->emit_stderr(QStringLiteral("stderr line"));
  raw_transport->emit_error(error);

  REQUIRE(captured_message.has_value());
  REQUIRE(*captured_message == message);
  REQUIRE(captured_stderr.has_value());
  REQUIRE(*captured_stderr == QStringLiteral("stderr line"));
  REQUIRE(captured_error.has_value());
  REQUIRE(*captured_error == error);
}

TEST_CASE("transport connection reports missing transport", "[transport][connection]")
{
  transport::TransportConnection connection(nullptr);
  std::optional<transport::TransportError> captured_error;
  REQUIRE(QObject::connect(
      &connection,
      &transport::TransportConnection::errorOccurred,
      [&captured_error](const transport::TransportError& error) { captured_error = error; }));

  REQUIRE_FALSE(connection.connect_to(make_profile()));

  REQUIRE(captured_error.has_value());
  REQUIRE(captured_error->category == transport::TransportErrorCategory::Unknown);
  REQUIRE(connection.state() == transport::TransportState::Failed);
}
