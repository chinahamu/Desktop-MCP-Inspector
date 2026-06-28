#pragma once

#include "TimelineEvent.hpp"
#include "TimelineStore.hpp"

#include <QAbstractTableModel>
#include <QModelIndex>
#include <QObject>
#include <QVariant>
#include <QtTypes>

namespace timeline {

class TimelineModel final : public QAbstractTableModel
{
  Q_OBJECT

public:
  enum Column {
    TimestampColumn = 0,
    DirectionColumn,
    KindColumn,
    MethodColumn,
    StatusColumn,
    DurationColumn,
    SummaryColumn,
    ColumnCount,
  };

  explicit TimelineModel(QObject* parent = nullptr);
  explicit TimelineModel(TimelineStore* store, QObject* parent = nullptr);

  [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex{}) const override;
  [[nodiscard]] int columnCount(const QModelIndex& parent = QModelIndex{}) const override;
  [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  [[nodiscard]] QVariant headerData(
      int section,
      Qt::Orientation orientation,
      int role = Qt::DisplayRole) const override;

  void set_store(TimelineStore* store);
  [[nodiscard]] TimelineStore* store() const;
  [[nodiscard]] const TimelineEvent* event_at(int row) const;

private:
  void connect_store();
  [[nodiscard]] QVariant display_data(const TimelineEvent& event, int column) const;

  TimelineStore* store_ = nullptr;
};

} // namespace timeline
