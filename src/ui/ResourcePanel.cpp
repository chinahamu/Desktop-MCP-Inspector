#include "ResourcePanel.hpp"

#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>

#include <utility>

namespace ui {
namespace {

[[nodiscard]] QString format_json(const QJsonObject& object)
{
  return QString::fromUtf8(QJsonDocument{object}.toJson(QJsonDocument::Indented));
}

[[nodiscard]] QJsonObject read_requested_result(const mcp::McpResource& resource)
{
  return QJsonObject{
      {QStringLiteral("status"), QStringLiteral("read-requested")},
      {QStringLiteral("method"), QString::fromUtf8(mcp::kMcpResourcesReadMethod)},
      {QStringLiteral("params"), QJsonObject{{QStringLiteral("uri"), resource.uri}}},
  };
}

} // namespace

ResourcePanel::ResourcePanel(QWidget* parent)
  : QWidget(parent)
{
  auto* root_layout = new QVBoxLayout(this);
  root_layout->setContentsMargins(0, 0, 0, 0);
  root_layout->setSpacing(8);

  auto* splitter = new QSplitter(Qt::Horizontal, this);
  auto* list_container = new QWidget(splitter);
  auto* list_layout = new QVBoxLayout(list_container);
  list_layout->setContentsMargins(0, 0, 0, 0);
  setup_resource_list(list_layout);

  auto* detail_container = new QWidget(splitter);
  auto* detail_layout = new QVBoxLayout(detail_container);
  detail_layout->setContentsMargins(0, 0, 0, 0);
  setup_resource_detail(detail_layout);

  splitter->addWidget(list_container);
  splitter->addWidget(detail_container);
  splitter->setStretchFactor(0, 1);
  splitter->setStretchFactor(1, 3);
  root_layout->addWidget(splitter, 1);

  connect_controls();
  update_detail_for_current_resource();
}

void ResourcePanel::set_resources(std::vector<mcp::McpResource> resources)
{
  resources_ = std::move(resources);
  resources_list_->clear();

  for (std::size_t index = 0; index < resources_.size(); ++index) {
    const auto& resource = resources_.at(index);
    auto* item = new QListWidgetItem(resource.name, resources_list_);
    item->setData(Qt::UserRole, static_cast<int>(index));
    item->setToolTip(resource.uri);
  }

  if (!resources_.empty()) {
    resources_list_->setCurrentRow(0);
  } else {
    update_detail_for_current_resource();
  }
}

void ResourcePanel::clear_resources()
{
  resources_.clear();
  resources_list_->clear();
  update_detail_for_current_resource();
}

void ResourcePanel::set_read_result(const QJsonObject& result)
{
  set_preview_json(result);
}

void ResourcePanel::set_read_error(const QString& message)
{
  set_preview_json(QJsonObject{
      {QStringLiteral("status"), QStringLiteral("error")},
      {QStringLiteral("message"), message},
  });
}

std::optional<mcp::McpResource> ResourcePanel::selected_resource() const
{
  const auto index = selected_resource_index();
  if (!index.has_value()) {
    return std::nullopt;
  }

  return resources_.at(static_cast<std::size_t>(*index));
}

void ResourcePanel::setup_resource_list(QVBoxLayout* root_layout)
{
  root_layout->addWidget(new QLabel(tr("Resources"), this));

  resources_list_ = new QListWidget(this);
  resources_list_->setObjectName(QStringLiteral("resourcesList"));
  resources_list_->setSelectionMode(QAbstractItemView::SingleSelection);
  root_layout->addWidget(resources_list_, 1);
}

void ResourcePanel::setup_resource_detail(QVBoxLayout* root_layout)
{
  root_layout->addWidget(new QLabel(tr("Metadata"), this));
  metadata_view_ = new QPlainTextEdit(this);
  metadata_view_->setObjectName(QStringLiteral("resourceMetadata"));
  metadata_view_->setReadOnly(true);
  metadata_view_->setMaximumBlockCount(400);
  root_layout->addWidget(metadata_view_, 1);

  resource_status_ = new QLabel(tr("Select a resource to inspect metadata and preview content."), this);
  resource_status_->setObjectName(QStringLiteral("resourceStatus"));

  read_button_ = new QPushButton(tr("Read resource"), this);
  read_button_->setObjectName(QStringLiteral("resourceReadButton"));

  auto* actions = new QHBoxLayout();
  actions->setContentsMargins(0, 0, 0, 0);
  actions->addWidget(resource_status_, 1);
  actions->addWidget(read_button_);
  root_layout->addLayout(actions);

  root_layout->addWidget(new QLabel(tr("Content preview"), this));
  preview_view_ = new QPlainTextEdit(this);
  preview_view_->setObjectName(QStringLiteral("resourcePreview"));
  preview_view_->setReadOnly(true);
  preview_view_->setPlaceholderText(tr("resources/read results and errors will appear here."));
  root_layout->addWidget(preview_view_, 1);
}

void ResourcePanel::connect_controls()
{
  connect(resources_list_, &QListWidget::currentRowChanged, this, [this](int) {
    update_detail_for_current_resource();
  });
  connect(read_button_, &QPushButton::clicked, this, &ResourcePanel::read_selected_resource);
}

void ResourcePanel::update_detail_for_current_resource()
{
  const auto resource = selected_resource();
  read_button_->setEnabled(resource.has_value());

  if (!resource.has_value()) {
    metadata_view_->setPlainText(tr("Select a resource to inspect its metadata."));
    resource_status_->setText(tr("No resource selected."));
    preview_view_->clear();
    return;
  }

  metadata_view_->setPlainText(format_json(mcp::to_json_object(*resource)));
  resource_status_->setText(tr("URI: %1").arg(resource->uri));
}

void ResourcePanel::read_selected_resource()
{
  const auto resource = selected_resource();
  if (!resource.has_value()) {
    set_read_error(tr("Select a resource before running resources/read."));
    return;
  }

  set_read_result(read_requested_result(*resource));
  emit resourceReadRequested(resource->uri);
}

std::optional<int> ResourcePanel::selected_resource_index() const
{
  const auto* item = resources_list_->currentItem();
  if (item == nullptr) {
    return std::nullopt;
  }

  const auto value = item->data(Qt::UserRole);
  if (!value.isValid()) {
    return std::nullopt;
  }

  const auto index = value.toInt();
  if (index < 0 || index >= static_cast<int>(resources_.size())) {
    return std::nullopt;
  }

  return index;
}

void ResourcePanel::set_preview_json(const QJsonObject& object)
{
  preview_view_->setPlainText(format_json(object));
}

} // namespace ui
