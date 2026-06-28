#include "TimelineStore.hpp"

#include <stdexcept>
#include <utility>

namespace timeline {

TimelineStore::TimelineStore(QObject* parent)
  : QObject(parent)
{
}

qsizetype TimelineStore::size() const
{
  return static_cast<qsizetype>(events_.size());
}

bool TimelineStore::empty() const
{
  return events_.empty();
}

const TimelineEvent& TimelineStore::at(qsizetype index) const
{
  if (index < 0 || index >= size()) {
    throw std::out_of_range{"timeline event index is out of range"};
  }

  return events_.at(static_cast<std::size_t>(index));
}

const std::vector<TimelineEvent>& TimelineStore::events() const
{
  return events_;
}

void TimelineStore::append(TimelineEvent event)
{
  if (event.event_id.trimmed().isEmpty()) {
    event.event_id = make_timeline_event_id();
  }

  if (!event.timestamp.isValid()) {
    event.timestamp = QDateTime::currentDateTimeUtc();
  }

  const auto row = size();
  emit eventAboutToBeAppended(row);
  events_.push_back(std::move(event));
  emit eventAppended(row);
}

void TimelineStore::append_many(std::vector<TimelineEvent> events)
{
  if (events.empty()) {
    return;
  }

  emit eventsAboutToBeReset();
  for (auto& event : events) {
    if (event.event_id.trimmed().isEmpty()) {
      event.event_id = make_timeline_event_id();
    }
    if (!event.timestamp.isValid()) {
      event.timestamp = QDateTime::currentDateTimeUtc();
    }
    events_.push_back(std::move(event));
  }
  emit eventsReset();
}

void TimelineStore::clear()
{
  if (events_.empty()) {
    return;
  }

  emit eventsAboutToBeReset();
  events_.clear();
  emit eventsReset();
}

} // namespace timeline
