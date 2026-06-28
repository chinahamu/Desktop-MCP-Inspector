#include <catch2/catch_test_macros.hpp>

#include "StdioProtocolViolation.hpp"

#include <QJsonObject>
#include <QString>

TEST_CASE("stdio protocol violation detector accepts valid JSON-RPC requests", "[transport][stdio]")
{
  const QJsonObject message{
      {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
      {QStringLiteral("id"), 1},
      {QStringLiteral("method"), QStringLiteral("tools/list")},
  };

  REQUIRE_FALSE(transport::detect_stdio_protocol_violation(message).has_value());
}

TEST_CASE("stdio protocol violation detector accepts valid JSON-RPC responses", "[transport][stdio]")
{
  const QJsonObject message{
      {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
      {QStringLiteral("id"), QStringLiteral("request-1")},
      {QStringLiteral("result"), QJsonObject{{QStringLiteral("ok"), true}}},
  };

  REQUIRE_FALSE(transport::detect_stdio_protocol_violation(message).has_value());
}

TEST_CASE("stdio protocol violation detector rejects missing JSON-RPC version", "[transport][stdio]")
{
  const QJsonObject message{
      {QStringLiteral("id"), 1},
      {QStringLiteral("method"), QStringLiteral("tools/list")},
  };

  const auto violation = transport::detect_stdio_protocol_violation(message);

  REQUIRE(violation.has_value());
  REQUIRE(violation->code == QStringLiteral("invalid_version"));
  REQUIRE(violation->details.value(QStringLiteral("code")).toString() == QStringLiteral("invalid_version"));
  REQUIRE_FALSE(violation->details.value(QStringLiteral("raw")).toString().isEmpty());
}

TEST_CASE("stdio protocol violation detector rejects invalid method messages", "[transport][stdio]")
{
  const QJsonObject message{
      {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
      {QStringLiteral("id"), 1},
      {QStringLiteral("method"), QStringLiteral("")},
  };

  const auto violation = transport::detect_stdio_protocol_violation(message);

  REQUIRE(violation.has_value());
  REQUIRE(violation->code == QStringLiteral("invalid_method"));
  REQUIRE(violation->message == QStringLiteral("JSON-RPC method must be a non-empty string"));
}

TEST_CASE("stdio protocol violation detector rejects invalid response shape", "[transport][stdio]")
{
  const QJsonObject message{
      {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
      {QStringLiteral("id"), 1},
      {QStringLiteral("result"), QJsonObject{}},
      {QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), -32603}, {QStringLiteral("message"), QStringLiteral("boom")}}},
  };

  const auto violation = transport::detect_stdio_protocol_violation(message);

  REQUIRE(violation.has_value());
  REQUIRE(violation->code == QStringLiteral("invalid_response"));
}

TEST_CASE("stdio protocol violation detector rejects malformed error responses", "[transport][stdio]")
{
  const QJsonObject message{
      {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
      {QStringLiteral("id"), 1},
      {QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), QStringLiteral("bad")}}},
  };

  const auto violation = transport::detect_stdio_protocol_violation(message);

  REQUIRE(violation.has_value());
  REQUIRE(violation->code == QStringLiteral("invalid_error"));
}
