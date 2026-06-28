#include <catch2/catch_test_macros.hpp>

#include "StdioJsonLineParser.hpp"

#include <QJsonObject>
#include <QString>

TEST_CASE("stdio JSON line parser emits complete newline-delimited objects", "[transport][stdio]")
{
  transport::StdioJsonLineParser parser;

  const auto result = parser.append(
      QByteArray{"{\"jsonrpc\":\"2.0\",\"id\":1}\n{\"method\":\"notifications/initialized\"}\n"});

  REQUIRE(result.errors.empty());
  REQUIRE(result.messages.size() == 2);
  REQUIRE(result.messages.at(0).value(QStringLiteral("jsonrpc")).toString() == QStringLiteral("2.0"));
  REQUIRE(result.messages.at(0).value(QStringLiteral("id")).toInt() == 1);
  REQUIRE(result.messages.at(1).value(QStringLiteral("method")).toString() == QStringLiteral("notifications/initialized"));
  REQUIRE_FALSE(parser.has_pending_line());
}

TEST_CASE("stdio JSON line parser buffers partial stdout chunks", "[transport][stdio]")
{
  transport::StdioJsonLineParser parser;

  const auto first_result = parser.append(QByteArray{"{\"jsonrpc\":\"2.0\","});

  REQUIRE(first_result.messages.empty());
  REQUIRE(first_result.errors.empty());
  REQUIRE(parser.has_pending_line());

  const auto second_result = parser.append(QByteArray{"\"id\":42}\n"});

  REQUIRE(second_result.errors.empty());
  REQUIRE(second_result.messages.size() == 1);
  REQUIRE(second_result.messages.front().value(QStringLiteral("id")).toInt() == 42);
  REQUIRE_FALSE(parser.has_pending_line());
}

TEST_CASE("stdio JSON line parser accepts CRLF and ignores blank lines", "[transport][stdio]")
{
  transport::StdioJsonLineParser parser;

  const auto result = parser.append(QByteArray{"\r\n{\"method\":\"tools/list\"}\r\n\n"});

  REQUIRE(result.errors.empty());
  REQUIRE(result.messages.size() == 1);
  REQUIRE(result.messages.front().value(QStringLiteral("method")).toString() == QStringLiteral("tools/list"));
}

TEST_CASE("stdio JSON line parser reports invalid JSON and continues", "[transport][stdio]")
{
  transport::StdioJsonLineParser parser;

  const auto result = parser.append(QByteArray{"not json\n[1,2,3]\n{\"ok\":true}\n"});

  REQUIRE(result.messages.size() == 1);
  REQUIRE(result.messages.front().value(QStringLiteral("ok")).toBool());
  REQUIRE(result.errors.size() == 2);
  REQUIRE(result.errors.at(0).raw_line == QByteArray{"not json"});
  REQUIRE_FALSE(result.errors.at(0).message.isEmpty());
  REQUIRE(result.errors.at(1).raw_line == QByteArray{"[1,2,3]"});
  REQUIRE(result.errors.at(1).message == QStringLiteral("stdout line must be a JSON object"));
}

TEST_CASE("stdio JSON line parser can clear buffered partial data", "[transport][stdio]")
{
  transport::StdioJsonLineParser parser;

  parser.append(QByteArray{"{\"jsonrpc\":"});
  REQUIRE(parser.has_pending_line());

  parser.clear();

  REQUIRE_FALSE(parser.has_pending_line());
  REQUIRE(parser.pending_line().isEmpty());
}
