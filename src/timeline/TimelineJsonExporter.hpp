#pragma once

#include "TimelineEvent.hpp"

#include <QJsonArray>
#include <QJsonDocument>

#include <vector>

namespace timeline {

[[nodiscard]] QJsonArray export_events_to_json_array(const std::vector<TimelineEvent>& events);
[[nodiscard]] QJsonDocument export_events_to_json_document(const std::vector<TimelineEvent>& events);

} // namespace timeline
