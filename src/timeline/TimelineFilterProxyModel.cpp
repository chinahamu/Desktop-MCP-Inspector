#include "TimelineFilterProxyModel.hpp"

#include "TimelineModel.hpp"

#include <QtGlobal>

#include <utility>

namespace timeline {

TimelineFilterProxyModel::TimelineFilterProxyModel(QObject* parent)
  : QSortFilterProxyModel(parent)
{
  setDynamicSortFilter(true);
}

void TimelineFilterProxyModel::set_method_filter(QString method_filter)
{
  if (method_filter_ == method_filter) {
    return;
  }

  method_filter_ = std::move(method_filter);
  refresh_filter();
}

void TimelineFilterProxyModel::set_status_filter(std::optional<TimelineStatus> status)
{
  if (status_filter_ == status) {
    return;
  }

  status_filter_ = status;
  refresh_filter();
}

void TimelineFilterProxyModel::set_direction_filter(std::optional<TimelineDirection> direction)
{
  if (direction_filter_ == direction) {
    return;
  }

  direction_filter_ = direction;
  refresh_filter();
}

void TimelineFilterProxyModel::clear_filters()
{
  method_filter_.clear();
  status_filter_.reset();
  direction_filter_.reset();
  refresh_filter();
}

QString TimelineFilterProxyModel::method_filter() const
{
  return method_filter_;
}

std::optional<TimelineStatus> TimelineFilterProxyModel::status_filter() const
{
  return status_filter_;
}

std::optional<TimelineDirection> TimelineFilterProxyModel::direction_filter() const
{
  return direction_filter_;
}

bool TimelineFilterProxyModel::filterAcceptsRow(int source_row, const QModelIndex& source_parent) const
{
  const auto* timeline_model = qobject_cast<const TimelineModel*>(sourceModel());
  if (timeline_model == nullptr || source_parent.isValid()) {
    return false;
  }

  const auto* event = timeline_model->event_at(source_row);
  if (event == nullptr) {
    return false;
  }

  if (!method_filter_.trimmed().isEmpty()
      && !event->method.contains(method_filter_.trimmed(), Qt::CaseInsensitive)) {
    return false;
  }

  if (status_filter_.has_value() && event->status != *status_filter_) {
    return false;
  }

  if (direction_filter_.has_value() && event->direction != *direction_filter_) {
    return false;
  }

  return true;
}

void TimelineFilterProxyModel::refresh_filter()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
  beginFilterChange();
  endFilterChange(QSortFilterProxyModel::Direction::Rows);
#else
  invalidateFilter();
#endif
}

} // namespace timeline
