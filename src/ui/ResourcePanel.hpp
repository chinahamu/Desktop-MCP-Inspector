#pragma once

#include "McpResource.hpp"

#include <QJsonObject>
#include <QString>
#include <QWidget>

#include <optional>
#include <vector>

class QLabel;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QVBoxLayout;

namespace ui {

class ResourcePanel final : public QWidget
{
  Q_OBJECT

public:
  explicit ResourcePanel(QWidget* parent = nullptr);

  void set_resources(std::vector<mcp::McpResource> resources);
  void clear_resources();
  void set_read_result(const QJsonObject& result);
  void set_read_error(const QString& message);

  [[nodiscard]] std::optional<mcp::McpResource> selected_resource() const;

signals:
  void resourceReadRequested(const QString& uri);

private:
  void setup_resource_list(QVBoxLayout* root_layout);
  void setup_resource_detail(QVBoxLayout* root_layout);
  void connect_controls();
  void update_detail_for_current_resource();
  void read_selected_resource();
  [[nodiscard]] std::optional<int> selected_resource_index() const;
  void set_preview_json(const QJsonObject& object);

  std::vector<mcp::McpResource> resources_;
  QListWidget* resources_list_ = nullptr;
  QPlainTextEdit* metadata_view_ = nullptr;
  QPlainTextEdit* preview_view_ = nullptr;
  QLabel* resource_status_ = nullptr;
  QPushButton* read_button_ = nullptr;
};

} // namespace ui
