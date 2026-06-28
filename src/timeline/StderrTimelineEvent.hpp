#pragma once

#include "TimelineEvent.hpp"

#include <QDateTime>
#include <QString>

namespace timeline {

[[nodiscard]] TimelineEvent stderr_line_event(
    QString line,
    QDateTime timestamp = QDateTime::currentDateTimeUtc());

} // namespace timeline
