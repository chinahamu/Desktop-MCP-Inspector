#include <catch2/catch_test_macros.hpp>

#include "McpClient.hpp"
#include "McpTypes.hpp"

#include <QDateTime>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include <optional>
#include <stdexcept>

TEST_CASE("pending request records timeout metadata", "[mcp][client]")
{
  mcp::PendingRequestStore store;

  const auto pending = store.register_request(
      QStringLiteral("tools/list"),
      std::nullopt,
      std::optional<qint64>{1500});

  REQUIRE(pending.has_timeout());
  REQUIRE(pending.timeout_ms == std::optional<qint64>{1500});
  REQUIRE(pending.deadline_at().has_value());
  REQUIRE(*pending.deadline_at() == pending.created_at.addMSecs(1500));
  REQUIRE_FALSE(pending.is_timed_out(pending.created_at.addMSecs(1499)));
  REQUIRE(pending.is_timed_out(pending.created_at.addMSecs(1500)));
}

TEST_CASE("pending request store validates timeout values", "[mcp][client]")
{
  mcp::PendingRequestStore store;

  REQUIRE_THROWS_AS(
      store.register_request(QStringLiteral("initialize"), std::nullopt, std::optional<qint64>{0}),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      store.make_request(QStringLiteral("initialize"), std::nullopt, std::optional<qint64>{-1}),
      std::invalid_argument);
}

TEST_CASE("pending request store can create timed JSON-RPC requests", "[mcp][client]")
{
  mcp::PendingRequestStore store;

  const auto request = store.make_request(
      QStringLiteral("tools/call"),
      QJsonValue{QJsonObject{{QStringLiteral("name"), QStringLiteral("echo")}}},
      std::optional<qint64>{5000});

  REQUIRE(request.id == mcp::RequestId::number(1));
  REQUIRE(store.contains(request.id));

  const auto pending = store.find(request.id);
  REQUIRE(pending.has_value());
  REQUIRE(pending->timeout_ms == std::optional<qint64>{5000});
  REQUIRE_FALSE(store.has_timed_out(request.id, pending->created_at.addMSecs(4999)));
  REQUIRE(store.has_timed_out(request.id, pending->created_at.addMSecs(5000)));
}

TEST_CASE("pending request store expires timed out requests", "[mcp][client]")
{
  mcp::PendingRequestStore store;

  const auto short_timeout = store.register_request(
      QStringLiteral("resources/list"),
      std::nullopt,
      std::optional<qint64>{10});
  const auto long_timeout = store.register_request(
      QStringLiteral("tools/list"),
      std::nullopt,
      std::optional<qint64>{1000});
  const auto no_timeout = store.register_request(QStringLiteral("prompts/list"));

  const auto expired = store.expire_timed_out_requests(short_timeout.created_at.addMSecs(10));

  REQUIRE(expired.size() == 1);
  REQUIRE(expired.front().id == short_timeout.id);
  REQUIRE_FALSE(store.contains(short_timeout.id));
  REQUIRE(store.contains(long_timeout.id));
  REQUIRE(store.contains(no_timeout.id));
  REQUIRE(store.size() == 2);
}

TEST_CASE("pending request store cancels and removes matching request", "[mcp][client]")
{
  mcp::PendingRequestStore store;
  const auto pending = store.register_request(QStringLiteral("tools/call"));

  const auto cancelled = store.cancel(pending.id);

  REQUIRE(cancelled.has_value());
  REQUIRE(cancelled->id == pending.id);
  REQUIRE(cancelled->method == QStringLiteral("tools/call"));
  REQUIRE_FALSE(store.contains(pending.id));
  REQUIRE(store.empty());
  REQUIRE_FALSE(store.cancel(pending.id).has_value());
}
