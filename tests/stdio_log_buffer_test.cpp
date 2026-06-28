#include <catch2/catch_test_macros.hpp>

#include "StdioLogBuffer.hpp"

#include <QString>

TEST_CASE("stdio log buffer emits complete stderr lines", "[transport][stdio]")
{
  transport::StdioLogBuffer buffer;

  const auto lines = buffer.append(QByteArray{"first line\nsecond line\n"});

  REQUIRE(lines.size() == 2);
  REQUIRE(lines.at(0) == QStringLiteral("first line"));
  REQUIRE(lines.at(1) == QStringLiteral("second line"));
  REQUIRE_FALSE(buffer.has_pending_line());
}

TEST_CASE("stdio log buffer holds partial stderr lines", "[transport][stdio]")
{
  transport::StdioLogBuffer buffer;

  const auto first_lines = buffer.append(QByteArray{"partial"});

  REQUIRE(first_lines.empty());
  REQUIRE(buffer.has_pending_line());
  REQUIRE(buffer.pending_line() == QByteArray{"partial"});

  const auto second_lines = buffer.append(QByteArray{" log\n"});

  REQUIRE(second_lines.size() == 1);
  REQUIRE(second_lines.front() == QStringLiteral("partial log"));
  REQUIRE_FALSE(buffer.has_pending_line());
}

TEST_CASE("stdio log buffer trims CR from CRLF stderr lines", "[transport][stdio]")
{
  transport::StdioLogBuffer buffer;

  const auto lines = buffer.append(QByteArray{"windows line\r\nplain line\n"});

  REQUIRE(lines.size() == 2);
  REQUIRE(lines.at(0) == QStringLiteral("windows line"));
  REQUIRE(lines.at(1) == QStringLiteral("plain line"));
}

TEST_CASE("stdio log buffer preserves blank stderr lines", "[transport][stdio]")
{
  transport::StdioLogBuffer buffer;

  const auto lines = buffer.append(QByteArray{"before\n\nafter\n"});

  REQUIRE(lines.size() == 3);
  REQUIRE(lines.at(0) == QStringLiteral("before"));
  REQUIRE(lines.at(1).isEmpty());
  REQUIRE(lines.at(2) == QStringLiteral("after"));
}

TEST_CASE("stdio log buffer flushes unterminated stderr tail", "[transport][stdio]")
{
  transport::StdioLogBuffer buffer;

  buffer.append(QByteArray{"unterminated tail"});
  const auto lines = buffer.flush();

  REQUIRE(lines.size() == 1);
  REQUIRE(lines.front() == QStringLiteral("unterminated tail"));
  REQUIRE_FALSE(buffer.has_pending_line());
}

TEST_CASE("stdio log buffer can be cleared", "[transport][stdio]")
{
  transport::StdioLogBuffer buffer;

  buffer.append(QByteArray{"pending"});
  REQUIRE(buffer.has_pending_line());

  buffer.clear();

  REQUIRE_FALSE(buffer.has_pending_line());
  REQUIRE(buffer.pending_line().isEmpty());
}
