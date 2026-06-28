#include <catch2/catch_test_macros.hpp>

#include "StdioProcessError.hpp"
#include "Transport.hpp"

#include <QJsonObject>
#include <QProcess>
#include <QString>

TEST_CASE("stdio process errors map to transport categories", "[transport][stdio]")
{
  REQUIRE(
      transport::stdio_process_error_category(QProcess::FailedToStart)
      == transport::TransportErrorCategory::StartFailed);
  REQUIRE(
      transport::stdio_process_error_category(QProcess::WriteError)
      == transport::TransportErrorCategory::WriteFailed);
  REQUIRE(
      transport::stdio_process_error_category(QProcess::ReadError)
      == transport::TransportErrorCategory::ReadFailed);
  REQUIRE(
      transport::stdio_process_error_category(QProcess::Timedout)
      == transport::TransportErrorCategory::Timeout);
  REQUIRE(
      transport::stdio_process_error_category(QProcess::Crashed)
      == transport::TransportErrorCategory::ProcessExited);
}

TEST_CASE("requested stdio stop is not treated as unexpected exit", "[transport][stdio]")
{
  const transport::StdioProcessExit process_exit{
      .exit_code = 0,
      .crashed = false,
      .requested_stop = true,
  };

  REQUIRE_FALSE(transport::is_unexpected_process_exit(process_exit));
  REQUIRE(transport::stdio_exit_message(process_exit) == QStringLiteral("stdio process stopped"));
}

TEST_CASE("unexpected stdio exit includes exit details", "[transport][stdio]")
{
  const transport::StdioProcessExit process_exit{
      .exit_code = 2,
      .crashed = false,
      .requested_stop = false,
  };

  const auto error = transport::stdio_exit_error(process_exit);

  REQUIRE(transport::is_unexpected_process_exit(process_exit));
  REQUIRE(error.category == transport::TransportErrorCategory::ProcessExited);
  REQUIRE(error.exit_code.has_value());
  REQUIRE(*error.exit_code == 2);
  REQUIRE(error.message == QStringLiteral("stdio process exited unexpectedly with code 2"));
  REQUIRE(error.details.has_value());
  REQUIRE(error.details->value(QStringLiteral("exit_code")).toInt() == 2);
  REQUIRE_FALSE(error.details->value(QStringLiteral("crashed")).toBool());
  REQUIRE_FALSE(error.details->value(QStringLiteral("requested_stop")).toBool());
}

TEST_CASE("crashed stdio exit is represented as process exited", "[transport][stdio]")
{
  const transport::StdioProcessExit process_exit{
      .exit_code = 1,
      .crashed = true,
      .requested_stop = false,
  };

  const auto error = transport::stdio_exit_error(process_exit);

  REQUIRE(transport::is_unexpected_process_exit(process_exit));
  REQUIRE(error.category == transport::TransportErrorCategory::ProcessExited);
  REQUIRE(error.message == QStringLiteral("stdio process crashed"));
  REQUIRE(error.details.has_value());
  REQUIRE(error.details->value(QStringLiteral("exit_code")).toInt() == 1);
  REQUIRE(error.details->value(QStringLiteral("crashed")).toBool());
}
