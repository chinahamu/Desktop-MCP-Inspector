#include "TimelinePanel.hpp"

#include "JsonDetailViewer.hpp"
#include "TimelineJsonExporter.hpp"

#include <QAbstractItemView>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIODevice>
#include <QItemSelectionModel>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QTableView>
#include <QVariant>
#include <QVBoxLayout>

#include <optional>
#include <utility>

namespace ui {
namespace {

template <typename Enum>
void add_optional_enum_item(QComboBox* combo_box, const QString& label, std::optional<Enum> value)
{
  combo_box->addItem(label, value.has_value() ? QVariant::fromValue(static_cast<int>(*value)) : QVariant{});
}

template <typename Enum>
[[nodiscard]] std::optional<Enum> optional_enum_from_combo(const QComboBox* combo_box)
{
  const auto data = combo_box->currentData();
  if (!data.isValid()) {
    return std::nullopt;
  }

  return static_cast<Enum>(data.toInt());
}

} // namespace

TimelinePanel::TimelinePanel(QWidget* parent)
  : QWidget(parent)
  , store_()
  , model_(&store_)
  , proxy_model_()
{
  proxy_model_.setSourceModel(&model_);

  auto* root_layout = new QVBoxLayout(this);
  root_layout->setContentsMargins(0, 0, 0, 0);
  root_layout->setSpacing(8);

  setup_filters(root_layout);
  setup_split_view(root_layout);
  connect_filter_controls();
  connect_selection();
}

void TimelinePanel::append_event(timeline::TimelineEvent event)
{
  store_.append(std::move(event));
}

void TimelinePanel::clear_events()
{
  store_.clear();
}

timeline::TimelineStore& TimelinePanel::store()
{
  return store_;
}

const timeline::TimelineStore& TimelinePanel::store() const
{
  return store_;
}

void TimelinePanel::setup_filters(QVBoxLayout* root_layout)
{
  auto* filters = new QHBoxLayout();
  filters->setContentsMargins(0, 0, 0, 0);

  method_filter_ = new QLineEdit(this);
  method_filter_->setPlaceholderText(tr("Filter method"));

  status_filter_ = new QComboBox(this);
  add_optional_enum_item<timeline::TimelineStatus>(status_filter_, tr("All statuses"), std::nullopt);
  add_optional_enum_item(
      status_filter_, tr("Pending"), std::optional{timeline::TimelineStatus::Pending});
  add_optional_enum_item(
      status_filter_, tr("Success"), std::optional{timeline::TimelineStatus::Success});
  add_optional_enum_item(status_filter_, tr("Error"), std::optional{timeline::TimelineStatus::Error});
  add_optional_enum_item(status_filter_, tr("Log"), std::optional{timeline::TimelineStatus::Log});

  direction_filter_ = new QComboBox(this);
  add_optional_enum_item<timeline::TimelineDirection>(direction_filter_, tr("All directions"), std::nullopt);
  add_optional_enum_item(
      direction_filter_, tr("Inbound"), std::optional{timeline::TimelineDirection::Inbound});
  add_optional_enum_item(
      direction_filter_, tr("Outbound"), std::optional{timeline::TimelineDirection::Outbound});
  add_optional_enum_item(
      direction_filter_, tr("Internal"), std::optional{timeline::TimelineDirection::Internal});

  export_button_ = new QPushButton(tr("Export JSON"), this);

  filters->addWidget(new QLabel(tr("Timeline"), this));
  filters->addWidget(method_filter_, 1);
  filters->addWidget(status_filter_);
  filters->addWidget(direction_filter_);
  filters->addWidget(export_button_);
  root_layout->addLayout(filters);
}

void TimelinePanel::setup_split_view(QVBoxLayout* root_layout)
{
  auto* splitter = new QSplitter(Qt::Vertical, this);

  table_ = new QTableView(splitter);
  table_->setModel(&proxy_model_);
  table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  table_->setSelectionMode(QAbstractItemView::SingleSelection);
  table_->setSortingEnabled(true);
  table_->horizontalHeader()->setStretchLastSection(true);
  table_->verticalHeader()->setVisible(false);

  detail_viewer_ = new JsonDetailViewer(splitter);

  splitter->addWidget(table_);
  splitter->addWidget(detail_viewer_);
  splitter->setStretchFactor(0, 3);
  splitter->setStretchFactor(1, 2);
  root_layout->addWidget(splitter, 1);
}

void TimelinePanel::connect_filter_controls()
{
  connect(method_filter_, &QLineEdit::textChanged, this, [this](const QString& text) {
    proxy_model_.set_method_filter(text);
  });
  connect(status_filter_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
    proxy_model_.set_status_filter(optional_enum_from_combo<timeline::TimelineStatus>(status_filter_));
  });
  connect(direction_filter_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
    proxy_model_.set_direction_filter(
        optional_enum_from_combo<timeline::TimelineDirection>(direction_filter_));
  });
  connect(export_button_, &QPushButton::clicked, this, &TimelinePanel::export_visible_events);
}

void TimelinePanel::connect_selection()
{
  connect(table_->selectionModel(), &QItemSelectionModel::currentRowChanged, this, [this] {
    update_detail_for_current_row();
  });
  connect(&proxy_model_, &QAbstractItemModel::modelReset, this, [this] { update_detail_for_current_row(); });
}

void TimelinePanel::update_detail_for_current_row()
{
  const auto proxy_index = table_->currentIndex();
  if (!proxy_index.isValid()) {
    detail_viewer_->set_placeholder(tr("Select a timeline row to inspect raw JSON."));
    return;
  }

  const auto source_index = proxy_model_.mapToSource(proxy_index);
  const auto* event = model_.event_at(source_index.row());
  if (event == nullptr) {
    detail_viewer_->set_placeholder(tr("Timeline event is no longer available."));
    return;
  }

  detail_viewer_->set_json(timeline::to_json_object(*event));
}

void TimelinePanel::export_visible_events()
{
  const auto path = QFileDialog::getSaveFileName(
      this,
      tr("Export timeline JSON"),
      QStringLiteral("timeline.json"),
      tr("JSON files (*.json);;All files (*)"));
  if (path.isEmpty()) {
    return;
  }

  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    QMessageBox::warning(this, tr("Export failed"), file.errorString());
    return;
  }

  file.write(timeline::export_events_to_json_document(visible_events()).toJson(QJsonDocument::Indented));
}

std::vector<timeline::TimelineEvent> TimelinePanel::visible_events() const
{
  std::vector<timeline::TimelineEvent> events;
  events.reserve(static_cast<std::size_t>(proxy_model_.rowCount()));

  for (int row = 0; row < proxy_model_.rowCount(); ++row) {
    const auto source_index = proxy_model_.mapToSource(proxy_model_.index(row, 0));
    if (const auto* event = model_.event_at(source_index.row()); event != nullptr) {
      events.push_back(*event);
    }
  }

  return events;
}

} // namespace ui
