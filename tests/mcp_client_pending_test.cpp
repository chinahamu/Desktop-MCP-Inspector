#include <catch2/catch_test_macros.hpp>

#include "McpClient.hpp"
#include "McpTypes.hpp"

#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include <stdexcept>

TEST_CASE("request id generator emits monotonic numeric ids", "[mcp][client]")
{
  mcp::RequestIdGenerator generator;

  const auto first = generator.next();
  const auto second = generator.next();

  REQUIRE(first == mcp::RequestId::number(1));
  REQUIRE(second == mcp::RequestId::number(2));
  REQUIRE(generator.peek_next_value() == 3);
}

TEST_CASE("request id generator validates initial and reset values", "[mcp][client]")
{
  REQUIRE_THROWS_AS(mcp::RequestIdGenerator{0}, std::invalid_argument);

  mcp::RequestIdGenerator generator;
  REQUIRE_THROWS_AS(generator.reset(0), std::invalid_argument);

  generator.reset(10);
  REQUIRE(generator.next() == mcp::RequestId::number(10));
  REQUIRE(generator.peek_next_value() == 11);
}

TEST_CASE("pending request store registers requests with generated ids", "[mcp][client]")
{
  mcp::PendingRequestStore store;

  const auto pending = store.register_request(
      QStringLiteral("tools/list"),
      QJsonValue{QJsonObject{{QStringLiteral("cursor"), QStringLiteral("next")}}});

  REQUIRE(pending.id == mcp::RequestId::number(1));
  REQUIRE(pending.method == QStringLiteral("tools/list"));
  REQUIRE(pending.params.has_value());
  REQUIRE(pending.params->toObject().value(QStringLiteral("cursor")).toString() == QStringLiteral("next"));
  REQUIRE(pending.created_at.isValid());
  REQUIRE(store.contains(pending.id));
  REQUIRE(store.size() == 1);
}

TEST_CASE("pending request store can create JSON-RPC requests", "[mcp][client]")
{
  mcp::PendingRequestStore store;

  const auto request = store.make_request(
      QStringLiteral("tools/call"),
      QJsonValue{QJsonObject{{QStringLiteral("name"), QStringLiteral("echo")}}});

  REQUIRE(request.id == mcp::RequestId::number(1));
  REQUIRE(request.method == QStringLiteral("tools/call"));
  REQUIRE(request.params.has_value());
  REQUIRE(request.params->toObject().value(QStringLiteral("name")).toString() == QStringLiteral("echo"));
  REQUIRE(store.contains(request.id));
}

TEST_CASE("pending request store completes and removes matching request", "[mcp][client]")
{
  mcp::PendingRequestStore store;
  const auto pending = store.register_request(QStringLiteral("initialize"));

  const auto completed = store.complete(pending.id);

  REQUIRE(completed.has_value());
  REQUIRE(completed->id == pending.id);
  REQUIRE(completed->method == QStringLiteral("initialize"));
  REQUIRE_FALSE(store.contains(pending.id));
  REQUIRE(store.empty());
  REQUIRE_FALSE(store.complete(pending.id).has_value());
}

TEST_CASE("pending request store finds and clears requests", "[mcp][client]")
{
  mcp::PendingRequestStore store{mcp::RequestIdGenerator{5}};
  const auto pending = store.register_request(QStringLiteral("resources/list"));

  const auto found = store.find(pending.id);
  REQUIRE(found.has_value());
  REQUIRE(found->id == mcp::RequestId::number(5));

  store.clear();
  REQUIRE(store.empty());
  REQUIRE_FALSE(store.find(pending.id).has_value());
}
