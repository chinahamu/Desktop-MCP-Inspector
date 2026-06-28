#include "TimelineJsonExporter.hpp"

namespace timeline {

QJsonArray export_events_to_json_array(const std::vector<TimelineEvent>& events)
{
  QJsonArray array;
  for (const auto& event : events) {
    array.append(to_json_object(event));
  }
  return array;
}

QJsonDocument export_events_to_json_document(const std::vector<TimelineEvent>& events)
{
  return QJsonDocument{export_events_to_json_array(events)};
}

} // namespace timeline
