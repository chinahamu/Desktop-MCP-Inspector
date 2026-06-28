#include <catch2/catch_test_macros.hpp>

#include "McpClient.hpp"
#include "StderrTimelineEvent.hpp"
#include "TimelineDuration.hpp"
#include "TimelineFilterProxyModel.hpp"
#include "TimelineJsonExporter.hpp"
#include "TimelineModel.hpp"
#include "TimelineStore.hpp"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QModelIndex>
#include <QString>

#include <optional>
#include <string>
#include <utility>

namespace {

[[nodiscard]] QDateTime fixed_time(qint64 ms)
{
  return QDateTime::fromMSecsSinceEpoch(ms, Qt::UTC);
}

[[nodiscard]] timeline::TimelineEvent sample_event(
    QString method,
    timeline::TimelineDirection direction = timeline::TimelineDirection::Outbound,
    timeline::TimelineStatus status = timeline::TimelineStatus::Pending)
{
  timeline::TimelineEvent event;
  event.event_id = QStringLiteral("event-%1").arg(method);
  event.timestamp = fixed_time(1000);
  event.direction = direction;
  event.kind = timeline::TimelineEventKind::Request;
  event.status = status;
  event.method = std::move(method);
  event.summary = QStringLiteral("sample summary");
  event.payload = QJsonObject{{QStringLiteral("sample"), true}};
  return event;
}

} // namespace

TEST_CASE("timeline events serialize core display fields", "[timeline]")
{
  auto event = sample_event(QStringLiteral("tools/list"));
  event.request_id = mcp::RequestId::number(42);
  event.duration_ms = 125;

  const auto json = timeline::to_json_object(event);

  REQUIRE(json.value(QStringLiteral("eventId")).toString() == QStringLiteral("event-tools/list"));
  REQUIRE(json.value(QStringLiteral("direction")).toString() == QStringLiteral("outbound"));
  REQUIRE(json.value(QStringLiteral("kind")).toString() == QStringLiteral("request"));
  REQUIRE(json.value(QStringLiteral("status")).toString() == QStringLiteral("pending"));
  REQUIRE(json.value(QStringLiteral("requestId")).toInteger() == 42);
  REQUIRE(json.value(QStringLiteral("durationMs")).toInteger() == 125);
}

TEST_CASE("timeline store and model expose appended events", "[timeline][model]")
{
  timeline::TimelineStore store;
  timeline::TimelineModel model{&store};

  store.append(sample_event(QStringLiteral("initialize")));

  REQUIRE(store.size() == 1);
  REQUIRE(model.rowCount() == 1);
  REQUIRE(model.columnCount() == timeline::TimelineModel::ColumnCount);
  REQUIRE(model.data(model.index(0, timeline::TimelineModel::MethodColumn)).toString()
          == QStringLiteral("initialize"));
  REQUIRE(model.data(model.index(0, timeline::TimelineModel::StatusColumn)).toString()
          == QStringLiteral("pending"));
}

TEST_CASE("timeline duration events calculate request response latency", "[timeline][duration]")
{
  mcp::PendingRequest pending;
  pending.id = mcp::RequestId::number(7);
  pending.method = QStringLiteral("tools/call");
  pending.created_at = fixed_time(1'000);
  pending.timeout_ms = 5'000;

  mcp::JsonRpcResponse response;
  response.id = pending.id;
  response.result = QJsonObject{{QStringLiteral("ok"), true}};

  const auto request_event = timeline::request_event_from_pending_request(pending, fixed_time(1'000));
  const auto response_event = timeline::response_event_from_pending_request(pending, response, fixed_time(2'250));

  REQUIRE(request_event.direction == timeline::TimelineDirection::Outbound);
  REQUIRE(request_event.status == timeline::TimelineStatus::Pending);
  REQUIRE(response_event.direction == timeline::TimelineDirection::Inbound);
  REQUIRE(response_event.status == timeline::TimelineStatus::Success);
  REQUIRE(response_event.duration_ms == std::optional<qint64>{1'250});
}

TEST_CASE("stderr lines become timeline log events", "[timeline][stderr]")
{
  const auto event = timeline::stderr_line_event(QStringLiteral("server booted"), fixed_time(10));

  REQUIRE(event.kind == timeline::TimelineEventKind::Stderr);
  REQUIRE(event.status == timeline::TimelineStatus::Log);
  REQUIRE(event.method == QStringLiteral("stderr"));
  REQUIRE(event.payload.value(QStringLiteral("line")).toString() == QStringLiteral("server booted"));
}

TEST_CASE("timeline filter narrows by method status and direction", "[timeline][filter]")
{
  timeline::TimelineStore store;
  timeline::TimelineModel model{&store};
  timeline::TimelineFilterProxyModel proxy;
  proxy.setSourceModel(&model);

  store.append(sample_event(QStringLiteral("tools/list"), timeline::TimelineDirection::Outbound));
  store.append(sample_event(
      QStringLiteral("resources/list"),
      timeline::TimelineDirection::Inbound,
      timeline::TimelineStatus::Success));
  store.append(sample_event(
      QStringLiteral("tools/call"),
      timeline::TimelineDirection::Inbound,
      timeline::TimelineStatus::Error));

  proxy.set_method_filter(QStringLiteral("tools"));
  REQUIRE(proxy.rowCount() == 2);

  proxy.set_status_filter(timeline::TimelineStatus::Error);
  REQUIRE(proxy.rowCount() == 1);

  proxy.set_direction_filter(timeline::TimelineDirection::Outbound);
  REQUIRE(proxy.rowCount() == 0);
}

TEST_CASE("timeline export has stable JSON snapshot shape", "[timeline][export]")
{
  std::vector<timeline::TimelineEvent> events;
  events.push_back(sample_event(QStringLiteral("initialize")));
  events.push_back(timeline::stderr_line_event(QStringLiteral("ready"), fixed_time(2'000)));

  const auto document = timeline::export_events_to_json_document(events);
  const auto array = document.array();

  REQUIRE(array.size() == 2);
  REQUIRE(array.at(0).toObject().value(QStringLiteral("method")).toString()
          == QStringLiteral("initialize"));
  REQUIRE(array.at(1).toObject().value(QStringLiteral("kind")).toString() == QStringLiteral("stderr"));
}
