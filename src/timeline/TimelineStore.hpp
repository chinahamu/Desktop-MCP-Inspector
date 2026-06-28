#pragma once

#include "TimelineEvent.hpp"

#include <QObject>
#include <QtTypes>

#include <vector>

namespace timeline {

class TimelineStore final : public QObject
{
  Q_OBJECT

public:
  explicit TimelineStore(QObject* parent = nullptr);

  [[nodiscard]] qsizetype size() const;
  [[nodiscard]] bool empty() const;
  [[nodiscard]] const TimelineEvent& at(qsizetype index) const;
  [[nodiscard]] const std::vector<TimelineEvent>& events() const;

  void append(TimelineEvent event);
  void append_many(std::vector<TimelineEvent> events);
  void clear();

signals:
  void eventAboutToBeAppended(qsizetype row);
  void eventAppended(qsizetype row);
  void eventsAboutToBeReset();
  void eventsReset();

private:
  std::vector<TimelineEvent> events_;
};

} // namespace timeline
