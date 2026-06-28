#include "TimelineModel.hpp"

namespace timeline {

TimelineModel::TimelineModel(QObject* parent)
  : QAbstractTableModel(parent)
{
}

TimelineModel::TimelineModel(TimelineStore* store, QObject* parent)
  : QAbstractTableModel(parent)
  , store_(store)
{
  connect_store();
}

int TimelineModel::rowCount(const QModelIndex& parent) const
{
  if (parent.isValid() || store_ == nullptr) {
    return 0;
  }

  return static_cast<int>(store_->size());
}

int TimelineModel::columnCount(const QModelIndex& parent) const
{
  return parent.isValid() ? 0 : ColumnCount;
}

QVariant TimelineModel::data(const QModelIndex& index, int role) const
{
  const auto* event = event_at(index.row());
  if (!index.isValid() || event == nullptr) {
    return {};
  }

  if (role == Qt::DisplayRole) {
    return display_data(*event, index.column());
  }

  if (role == Qt::ToolTipRole) {
    return event->summary;
  }

  return {};
}

QVariant TimelineModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
    return {};
  }

  switch (section) {
  case TimestampColumn:
    return tr("Time");
  case DirectionColumn:
    return tr("Direction");
  case KindColumn:
    return tr("Kind");
  case MethodColumn:
    return tr("Method");
  case StatusColumn:
    return tr("Status");
  case DurationColumn:
    return tr("Duration");
  case SummaryColumn:
    return tr("Summary");
  default:
    return {};
  }
}

void TimelineModel::set_store(TimelineStore* store)
{
  if (store_ == store) {
    return;
  }

  beginResetModel();
  if (store_ != nullptr) {
    disconnect(store_, nullptr, this, nullptr);
  }
  store_ = store;
  connect_store();
  endResetModel();
}

TimelineStore* TimelineModel::store() const
{
  return store_;
}

const TimelineEvent* TimelineModel::event_at(int row) const
{
  if (store_ == nullptr || row < 0 || row >= store_->size()) {
    return nullptr;
  }

  return &store_->at(row);
}

void TimelineModel::connect_store()
{
  if (store_ == nullptr) {
    return;
  }

  connect(store_, &TimelineStore::eventAboutToBeAppended, this, [this](qsizetype row) {
    beginInsertRows(QModelIndex{}, static_cast<int>(row), static_cast<int>(row));
  });
  connect(store_, &TimelineStore::eventAppended, this, [this](qsizetype) { endInsertRows(); });
  connect(store_, &TimelineStore::eventsAboutToBeReset, this, [this] { beginResetModel(); });
  connect(store_, &TimelineStore::eventsReset, this, [this] { endResetModel(); });
}

QVariant TimelineModel::display_data(const TimelineEvent& event, int column) const
{
  switch (column) {
  case TimestampColumn:
    return event.timestamp.toLocalTime().toString(QStringLiteral("HH:mm:ss.zzz"));
  case DirectionColumn:
    return timeline_direction_name(event.direction);
  case KindColumn:
    return timeline_event_kind_name(event.kind);
  case MethodColumn:
    return event.method;
  case StatusColumn:
    return timeline_status_name(event.status);
  case DurationColumn:
    return event.duration_ms.has_value() ? QStringLiteral("%1 ms").arg(*event.duration_ms) : QString{};
  case SummaryColumn:
    return event.summary;
  default:
    return {};
  }
}

} // namespace timeline
