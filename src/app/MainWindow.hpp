#pragma once

#include "McpInitialize.hpp"
#include "McpPrompt.hpp"
#include "McpResource.hpp"
#include "McpTool.hpp"
#include "ProfileStore.hpp"
#include "SecurityScanner.hpp"
#include "../compare/ServerSnapshot.hpp"

#include <QJsonObject>
#include <QMainWindow>
#include <QString>

#include <optional>
#include <vector>

class QAction;
class QLabel;

namespace transport {

class TransportConnection;

} // namespace transport

namespace ui {

class ComparePanel;
class ProfileEditor;
class PromptPanel;
class ResourcePanel;
class SecurityPanel;
class TimelinePanel;
class ToolPanel;

} // namespace ui

class MainWindow final : public QMainWindow
{
  Q_OBJECT

public:
  explicit MainWindow(QWidget* parent = nullptr);

private:
  using McpInitializeParams = mcp::McpInitializeParams;

  void setupCentralView();
  void setupStatusBar();
  void setupConnectionToolbar();
  void loadProfiles();
  void seedTimelineEvents();
  void seedToolPanel();
  void seedResourcePanel();
  void seedPromptPanel();
  void seedSecurityPanel();
  void seedComparePanel();

  void connectSelectedProfile();
  void disconnectSelectedProfile();
  void sendInitializeRequest();
  void sendToolsListRequest(std::optional<QString> cursor = std::nullopt);
  void sendToolsCallRequest(const QString& tool_name, const QJsonObject& arguments);
  void sendResourcesListRequest(std::optional<QString> cursor = std::nullopt);
  void sendResourcesReadRequest(const QString& uri);
  void sendPromptsListRequest(std::optional<QString> cursor = std::nullopt);
  void sendPromptsGetRequest(const QString& prompt_name, const QJsonObject& arguments);
  void runProfileLaunchTest(const config::ServerProfile& profile);
  void handleIncomingMessage(const QJsonObject& message);
  void handleInitializeResponse(const mcp::JsonRpcResponse& response);
  void handleToolsListResponse(const mcp::JsonRpcResponse& response);
  void handleToolsCallResponse(const mcp::JsonRpcResponse& response);
  void handleResourcesListResponse(const mcp::JsonRpcResponse& response);
  void handleResourcesReadResponse(const mcp::JsonRpcResponse& response);
  void handlePromptsListResponse(const mcp::JsonRpcResponse& response);
  void handlePromptsGetResponse(const mcp::JsonRpcResponse& response);
  void refreshSecurityScan();
  void refreshCompareDiff();
  void exportSecurityReport();
  void exportDiffReport();
  void updateConnectionActions();
  void clearLiveServerSurface();
  [[nodiscard]] compare::ServerSnapshot currentServerSnapshot() const;

  QLabel* statusLabel_ = nullptr;
  ui::ProfileEditor* profileEditor_ = nullptr;
  ui::TimelinePanel* timelinePanel_ = nullptr;
  ui::ToolPanel* toolsPanel_ = nullptr;
  ui::ResourcePanel* resourcesPanel_ = nullptr;
  ui::PromptPanel* promptsPanel_ = nullptr;
  ui::ComparePanel* comparePanel_ = nullptr;
  ui::SecurityPanel* securityPanel_ = nullptr;
  QAction* connectAction_ = nullptr;
  QAction* initializeAction_ = nullptr;
  QAction* disconnectAction_ = nullptr;
  transport::TransportConnection* connection_ = nullptr;
  config::ProfileStore profileStore_;
  mcp::PendingRequestStore pendingRequests_;
  std::optional<mcp::RequestId> initializeRequestId_;
  QJsonObject serverCapabilities_;
  QString serverDisplayName_;
  std::optional<compare::ServerSnapshot> previousSnapshot_;
  std::optional<compare::ServerSnapshot> currentSnapshot_;
  security::SecurityScanner securityScanner_;
  std::vector<mcp::McpTool> demoTools_;
  std::vector<mcp::McpResource> demoResources_;
  std::vector<mcp::McpPrompt> demoPrompts_;
};