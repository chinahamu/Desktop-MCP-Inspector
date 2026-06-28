#pragma once

#include "TimelineEvent.hpp"

#include <QSortFilterProxyModel>
#include <QString>

#include <optional>

namespace timeline {

class TimelineFilterProxyModel final : public QSortFilterProxyModel
{
  Q_OBJECT

public:
  explicit TimelineFilterProxyModel(QObject* parent = nullptr);

  void set_method_filter(QString method_filter);
  void set_status_filter(std::optional<TimelineStatus> status);
  void set_direction_filter(std::optional<TimelineDirection> direction);
  void clear_filters();

  [[nodiscard]] QString method_filter() const;
  [[nodiscard]] std::optional<TimelineStatus> status_filter() const;
  [[nodiscard]] std::optional<TimelineDirection> direction_filter() const;

protected:
  [[nodiscard]] bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override;

private:
  void refresh_filter();

  QString method_filter_;
  std::optional<TimelineStatus> status_filter_;
  std::optional<TimelineDirection> direction_filter_;
};

} // namespace timeline
