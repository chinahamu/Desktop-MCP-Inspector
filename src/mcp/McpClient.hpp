#pragma once

#include "McpTypes.hpp"

#include <QDateTime>
#include <QHash>
#include <QJsonValue>
#include <QString>
#include <QtTypes>

#include <optional>
#include <vector>

namespace mcp {

class RequestIdGenerator
{
public:
  explicit RequestIdGenerator(qint64 next_value = 1);

  [[nodiscard]] RequestId next();
  [[nodiscard]] qint64 peek_next_value() const;
  void reset(qint64 next_value = 1);

private:
  qint64 next_value_ = 1;
};

struct PendingRequest
{
  RequestId id;
  QString method;
  std::optional<QJsonValue> params;
  QDateTime created_at;
  std::optional<qint64> timeout_ms;

  [[nodiscard]] bool has_timeout() const;
  [[nodiscard]] std::optional<QDateTime> deadline_at() const;
  [[nodiscard]] bool is_timed_out(const QDateTime& now) const;
};

class PendingRequestStore
{
public:
  PendingRequestStore() = default;
  explicit PendingRequestStore(RequestIdGenerator generator);

  [[nodiscard]] PendingRequest register_request(
      QString method,
      std::optional<QJsonValue> params = std::nullopt,
      std::optional<qint64> timeout_ms = std::nullopt);
  [[nodiscard]] JsonRpcRequest make_request(
      QString method,
      std::optional<QJsonValue> params = std::nullopt,
      std::optional<qint64> timeout_ms = std::nullopt);

  [[nodiscard]] bool contains(const RequestId& id) const;
  [[nodiscard]] std::optional<PendingRequest> find(const RequestId& id) const;
  [[nodiscard]] bool has_timed_out(const RequestId& id, const QDateTime& now) const;
  [[nodiscard]] std::optional<PendingRequest> complete(const RequestId& id);
  [[nodiscard]] std::optional<PendingRequest> cancel(const RequestId& id);
  [[nodiscard]] std::vector<PendingRequest> expire_timed_out_requests(const QDateTime& now);

  void clear();
  [[nodiscard]] bool empty() const;
  [[nodiscard]] qsizetype size() const;

private:
  [[nodiscard]] static QString request_id_key(const RequestId& id);
  static void validate_timeout(std::optional<qint64> timeout_ms);

  RequestIdGenerator generator_;
  QHash<QString, PendingRequest> pending_requests_;
};

} // namespace mcp
