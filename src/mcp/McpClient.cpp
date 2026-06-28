#include "McpClient.hpp"

#include <QLatin1StringView>

#include <limits>
#include <stdexcept>
#include <utility>
#include <variant>

namespace mcp {
namespace {

template <class... Ts>
struct Overloaded : Ts...
{
  using Ts::operator()...;
};

template <class... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

[[nodiscard]] QDateTime current_utc_time()
{
  return QDateTime::currentDateTimeUtc();
}

} // namespace

RequestIdGenerator::RequestIdGenerator(qint64 next_value)
  : next_value_(next_value)
{
  if (next_value_ < 1) {
    throw std::invalid_argument{"RequestIdGenerator next value must be positive"};
  }
}

RequestId RequestIdGenerator::next()
{
  if (next_value_ == std::numeric_limits<qint64>::max()) {
    throw std::overflow_error{"RequestIdGenerator exhausted qint64 range"};
  }

  const auto value = next_value_;
  ++next_value_;
  return RequestId::number(value);
}

qint64 RequestIdGenerator::peek_next_value() const
{
  return next_value_;
}

void RequestIdGenerator::reset(qint64 next_value)
{
  if (next_value < 1) {
    throw std::invalid_argument{"RequestIdGenerator next value must be positive"};
  }

  next_value_ = next_value;
}

bool PendingRequest::has_timeout() const
{
  return timeout_ms.has_value();
}

std::optional<QDateTime> PendingRequest::deadline_at() const
{
  if (!timeout_ms.has_value()) {
    return std::nullopt;
  }

  return created_at.addMSecs(*timeout_ms);
}

bool PendingRequest::is_timed_out(const QDateTime& now) const
{
  const auto deadline = deadline_at();
  return deadline.has_value() && deadline->msecsTo(now) >= 0;
}

PendingRequestStore::PendingRequestStore(RequestIdGenerator generator)
  : generator_(std::move(generator))
{
}

PendingRequest PendingRequestStore::register_request(
    QString method,
    std::optional<QJsonValue> params,
    std::optional<qint64> timeout_ms)
{
  validate_timeout(timeout_ms);

  PendingRequest pending_request;
  pending_request.id = generator_.next();
  pending_request.method = std::move(method);
  pending_request.params = std::move(params);
  pending_request.created_at = current_utc_time();
  pending_request.timeout_ms = timeout_ms;

  pending_requests_.insert(request_id_key(pending_request.id), pending_request);
  return pending_request;
}

JsonRpcRequest PendingRequestStore::make_request(
    QString method,
    std::optional<QJsonValue> params,
    std::optional<qint64> timeout_ms)
{
  const auto pending_request = register_request(std::move(method), std::move(params), timeout_ms);

  JsonRpcRequest request;
  request.id = pending_request.id;
  request.method = pending_request.method;
  request.params = pending_request.params;
  return request;
}

bool PendingRequestStore::contains(const RequestId& id) const
{
  return pending_requests_.contains(request_id_key(id));
}

std::optional<PendingRequest> PendingRequestStore::find(const RequestId& id) const
{
  const auto iterator = pending_requests_.constFind(request_id_key(id));
  if (iterator == pending_requests_.constEnd()) {
    return std::nullopt;
  }

  return iterator.value();
}

bool PendingRequestStore::has_timed_out(const RequestId& id, const QDateTime& now) const
{
  const auto pending_request = find(id);
  return pending_request.has_value() && pending_request->is_timed_out(now);
}

std::optional<PendingRequest> PendingRequestStore::complete(const RequestId& id)
{
  const auto key = request_id_key(id);
  const auto iterator = pending_requests_.find(key);
  if (iterator == pending_requests_.end()) {
    return std::nullopt;
  }

  auto pending_request = iterator.value();
  pending_requests_.erase(iterator);
  return pending_request;
}

std::optional<PendingRequest> PendingRequestStore::cancel(const RequestId& id)
{
  return complete(id);
}

std::vector<PendingRequest> PendingRequestStore::expire_timed_out_requests(const QDateTime& now)
{
  std::vector<PendingRequest> expired_requests;

  for (auto iterator = pending_requests_.begin(); iterator != pending_requests_.end();) {
    if (iterator.value().is_timed_out(now)) {
      expired_requests.push_back(iterator.value());
      iterator = pending_requests_.erase(iterator);
    } else {
      ++iterator;
    }
  }

  return expired_requests;
}

void PendingRequestStore::clear()
{
  pending_requests_.clear();
}

bool PendingRequestStore::empty() const
{
  return pending_requests_.isEmpty();
}

qsizetype PendingRequestStore::size() const
{
  return pending_requests_.size();
}

QString PendingRequestStore::request_id_key(const RequestId& id)
{
  return std::visit(
      Overloaded{
          [](std::nullptr_t) { return QStringLiteral("null:"); },
          [](qint64 value) { return QStringLiteral("number:%1").arg(value); },
          [](const QString& value) { return QStringLiteral("string:%1").arg(value); },
      },
      id.value());
}

void PendingRequestStore::validate_timeout(std::optional<qint64> timeout_ms)
{
  if (timeout_ms.has_value() && *timeout_ms <= 0) {
    throw std::invalid_argument{"request timeout must be positive"};
  }
}

} // namespace mcp
