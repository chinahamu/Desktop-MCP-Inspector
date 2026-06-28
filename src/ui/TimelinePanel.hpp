#pragma once

#include "TimelineFilterProxyModel.hpp"
#include "TimelineModel.hpp"
#include "TimelineStore.hpp"

#include <QWidget>

#include <vector>

class QComboBox;
class QLineEdit;
class QModelIndex;
class QPushButton;
class QTableView;
class QVBoxLayout;

namespace ui {

class JsonDetailViewer;

class TimelinePanel final : public QWidget
{
  Q_OBJECT

public:
  explicit TimelinePanel(QWidget* parent = nullptr);

  void append_event(timeline::TimelineEvent event);
  void clear_events();
  [[nodiscard]] timeline::TimelineStore& store();
  [[nodiscard]] const timeline::TimelineStore& store() const;

private:
  void setup_filters(QVBoxLayout* root_layout);
  void setup_split_view(QVBoxLayout* root_layout);
  void connect_filter_controls();
  void connect_selection();
  void update_detail_for_current_row();
  void export_visible_events();
  [[nodiscard]] std::vector<timeline::TimelineEvent> visible_events() const;

  timeline::TimelineStore store_;
  timeline::TimelineModel model_;
  timeline::TimelineFilterProxyModel proxy_model_;
  QLineEdit* method_filter_ = nullptr;
  QComboBox* status_filter_ = nullptr;
  QComboBox* direction_filter_ = nullptr;
  QPushButton* export_button_ = nullptr;
  QTableView* table_ = nullptr;
  JsonDetailViewer* detail_viewer_ = nullptr;
};

} // namespace ui
