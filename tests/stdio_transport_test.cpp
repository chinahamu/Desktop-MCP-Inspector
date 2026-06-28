#include <catch2/catch_test_macros.hpp>

#include "ServerProfile.hpp"
#include "StdioTransport.hpp"
#include "Transport.hpp"

#include <QJsonObject>
#include <QObject>
#include <QString>

#include <optional>

TEST_CASE("stdio transport rejects an empty command", "[transport][stdio]")
{
  transport::StdioTransport transport;

  std::optional<transport::TransportError> captured_error;
  const auto error_connection = QObject::connect(
      &transport,
      &transport::Transport::errorOccurred,
      [&captured_error](const transport::TransportError& error) { captured_error = error; });
  REQUIRE(error_connection);

  config::ServerProfile profile;
  profile.command = QStringLiteral("   ");

  transport.start(profile);

  REQUIRE(transport.state() == transport::TransportState::Failed);
  REQUIRE(captured_error.has_value());
  REQUIRE(captured_error->category == transport::TransportErrorCategory::StartFailed);
}

TEST_CASE("stdio transport rejects stdin writes before start", "[transport][stdio]")
{
  transport::StdioTransport transport;

  std::optional<transport::TransportError> captured_error;
  const auto error_connection = QObject::connect(
      &transport,
      &transport::Transport::errorOccurred,
      [&captured_error](const transport::TransportError& error) { captured_error = error; });
  REQUIRE(error_connection);

  transport.send(QJsonObject{{QStringLiteral("jsonrpc"), QStringLiteral("2.0")}});

  REQUIRE(captured_error.has_value());
  REQUIRE(captured_error->category == transport::TransportErrorCategory::WriteFailed);
}

TEST_CASE("stdio server profile carries QProcess startup settings", "[transport][stdio]")
{
  config::ServerProfile profile;
  profile.id = QStringLiteral("echo-server");
  profile.name = QStringLiteral("Echo Server");
  profile.transport = config::ServerTransportKind::Stdio;
  profile.command = QStringLiteral("node");
  profile.args = QStringList{QStringLiteral("server.js"), QStringLiteral("--verbose")};
  profile.env.insert(QStringLiteral("MCP_LOG_LEVEL"), QStringLiteral("debug"));
  profile.cwd = QStringLiteral("/tmp/mcp-server");
  profile.timeout_ms = 30000;

  REQUIRE(config::is_stdio_profile(profile));
  REQUIRE(profile.command == QStringLiteral("node"));
  REQUIRE(profile.args.size() == 2);
  REQUIRE(profile.env.value(QStringLiteral("MCP_LOG_LEVEL")) == QStringLiteral("debug"));
  REQUIRE(profile.cwd == QStringLiteral("/tmp/mcp-server"));
  REQUIRE(profile.timeout_ms == std::optional<qint64>{30000});
}
