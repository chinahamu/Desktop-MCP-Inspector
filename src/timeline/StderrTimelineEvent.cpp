#include "StderrTimelineEvent.hpp"

#include <utility>

namespace timeline {

TimelineEvent stderr_line_event(QString line, QDateTime timestamp)
{
  TimelineEvent event;
  event.event_id = make_timeline_event_id();
  event.timestamp = timestamp;
  event.direction = TimelineDirection::Internal;
  event.kind = TimelineEventKind::Stderr;
  event.status = TimelineStatus::Log;
  event.method = QStringLiteral("stderr");
  event.summary = line;
  event.payload = QJsonObject{{QStringLiteral("line"), std::move(line)}};
  return event;
}

} // namespace timeline
