#include "MainWindow.hpp"

#include "AppSettings.hpp"
#include "ComparePanel.hpp"
#include "DiffMarkdownWriter.hpp"
#include "HTMLReportWriter.hpp"
#include "MarkdownReportWriter.hpp"
#include "McpInitialize.hpp"
#include "McpMessage.hpp"
#include "McpNotificationTimelineEvent.hpp"
#include "ProfileEditor.hpp"
#include "ProfileValidation.hpp"
#include "PromptPanel.hpp"
#include "ReportModel.hpp"
#include "ResourcePanel.hpp"
#include "SecurityPanel.hpp"
#include "StderrTimelineEvent.hpp"
#include "StdioTransport.hpp"
#include "TimelinePanel.hpp"
#include "ToolPanel.hpp"
#include "Transport.hpp"
#include "TransportConnection.hpp"
#include "../compare/ServerSnapshot.hpp"

#include <QAction>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

MainWindow::MainWindow(QWidget* parent)
  : QMainWindow(parent)
  , profileStore_(config::ProfileStore::sqlite(config::ProfileStore::default_sqlite_path()))
{
  const auto metadata = app::application_metadata();

  setWindowTitle(metadata.application_name);
  resize(1200, 800);

  setupCentralView();
  setupStatusBar();
  setupConnectionToolbar();
  loadProfiles();
  seedTimelineEvents();
  seedToolPanel();
  seedResourcePanel();
  seedPromptPanel();
  seedComparePanel();
  seedSecurityPanel();
}

void MainWindow::setupCentralView()
{
  auto* root = new QWidget(this);
  auto* layout = new QVBoxLayout(root);
  layout->setContentsMargins(16, 16, 16, 16);
  layout->setSpacing(12);

  const auto metadata = app::application_metadata();
  auto* title = new QLabel(metadata.application_name, root);
  title->setObjectName("welcomeTitle");
  title->setAlignment(Qt::AlignCenter);

  auto* description = new QLabel(
      tr("Inspect MCP traffic, configure server profiles, list tools/resources/prompts, compare server surfaces, validate JSON inputs, and review security findings."),
      root);
  description->setAlignment(Qt::AlignCenter);
  description->setWordWrap(true);

  auto* tabs = new QTabWidget(root);
  profileEditor_ = new ui::ProfileEditor(tabs);
  timelinePanel_ = new ui::TimelinePanel(tabs);
  toolsPanel_ = new ui::ToolPanel(tabs);
  resourcesPanel_ = new ui::ResourcePanel(tabs);
  promptsPanel_ = new ui::PromptPanel(tabs);
  comparePanel_ = new ui::ComparePanel(tabs);
  securityPanel_ = new ui::SecurityPanel(tabs);
  tabs->addTab(profileEditor_, tr("Profiles"));
  tabs->addTab(timelinePanel_, tr("Timeline"));
  tabs->addTab(toolsPanel_, tr("Tools"));
  tabs->addTab(resourcesPanel_, tr("Resources"));
  tabs->addTab(promptsPanel_, tr("Prompts"));
  tabs->addTab(comparePanel_, tr("Compare"));
  tabs->addTab(securityPanel_, tr("Security"));

  connect(profileEditor_, &ui::ProfileEditor::profileSaved, this, [this](const config::ServerProfile& profile) {
    statusBar()->showMessage(tr("Profile saved: %1").arg(profile.name), 3000);
    refreshSecurityScan();
    refreshCompareDiff();
  });
  connect(profileEditor_, &ui::ProfileEditor::profileSelected, this, [this](const config::ServerProfile&) {
    refreshSecurityScan();
    refreshCompareDiff();
  });
  connect(profileEditor_, &ui::ProfileEditor::profileDeleted, this, [this](const QString&) {
    statusBar()->showMessage(tr("Profile deleted"), 3000);
    refreshSecurityScan();
    refreshCompareDiff();
  });
  connect(profileEditor_, &ui::ProfileEditor::profileCloned, this, [this](const config::ServerProfile& profile) {
    statusBar()->showMessage(tr("Profile cloned: %1").arg(profile.name), 3000);
    refreshSecurityScan();
    refreshCompareDiff();
  });
  connect(profileEditor_, &ui::ProfileEditor::launchTestRequested, this, &MainWindow::runProfileLaunchTest);
  connect(comparePanel_, &ui::ComparePanel::compareRequested, this, &MainWindow::refreshCompareDiff);
  connect(comparePanel_, &ui::ComparePanel::exportReportRequested, this, &MainWindow::exportDiffReport);
  connect(securityPanel_, &ui::SecurityPanel::scanRequested, this, &MainWindow::refreshSecurityScan);
  connect(securityPanel_, &ui::SecurityPanel::exportReportRequested, this, &MainWindow::exportSecurityReport);

  layout->addWidget(title);
  layout->addWidget(description);
  layout->addWidget(tabs, 1);

  setCentralWidget(root);
}

void MainWindow::setupStatusBar()
{
  statusLabel_ = new QLabel(tr("Ready"), this);
  statusBar()->addPermanentWidget(statusLabel_);
  statusBar()->showMessage(tr("Desktop MCP Inspector initialized"));
}

void MainWindow::setupConnectionToolbar()
{
  auto* toolbar = addToolBar(tr("Connection"));
  toolbar->setObjectName(QStringLiteral("connectionToolbar"));
  toolbar->setMovable(false);

  connectAction_ = toolbar->addAction(tr("Connect"));
  connectAction_->setObjectName(QStringLiteral("connectProfileAction"));
  initializeAction_ = toolbar->addAction(tr("Initialize"));
  initializeAction_->setObjectName(QStringLiteral("initializeServerAction"));
  disconnectAction_ = toolbar->addAction(tr("Disconnect"));
  disconnectAction_->setObjectName(QStringLiteral("disconnectProfileAction"));

  connection_ = new transport::TransportConnection(std::make_unique<transport::StdioTransport>(), this);
  connect(connection_, &transport::TransportConnection::stateChanged, this, [this](transport::TransportState state) {
    const auto state_name = transport::transport_state_name(state);
    statusLabel_->setText(state_name);
    statusBar()->showMessage(tr("Connection state: %1").arg(state_name), 3000);
    if (state != transport::TransportState::Running) {
      pendingRequests_.clear();
      initializeRequestId_.reset();
    }
    updateConnectionActions();
  });
  connect(connection_, &transport::TransportConnection::errorOccurred, this, [this](const transport::TransportError& error) {
    statusBar()->showMessage(
        tr("Connection error: %1 - %2").arg(transport::transport_error_category_name(error.category), error.message),
        5000);
    updateConnectionActions();
  });
  connect(connection_, &transport::TransportConnection::stderrReceived, this, [this](const QString& text) {
    if (timelinePanel_ != nullptr) {
      timelinePanel_->append_event(timeline::stderr_line_event(text));
    }
  });
  connect(connection_, &transport::TransportConnection::messageReceived, this, &MainWindow::handleIncomingMessage);

  connect(connectAction_, &QAction::triggered, this, &MainWindow::connectSelectedProfile);
  connect(initializeAction_, &QAction::triggered, this, &MainWindow::sendInitializeRequest);
  connect(disconnectAction_, &QAction::triggered, this, &MainWindow::disconnectSelectedProfile);
  updateConnectionActions();
}

void MainWindow::loadProfiles()
{
  QString error_message;
  if (!profileStore_.load(&error_message)) {
    statusBar()->showMessage(tr("Failed to load profiles: %1").arg(error_message), 5000);
  }

  if (profileEditor_ != nullptr) {
    profileEditor_->set_store(&profileStore_);
  }
}

void MainWindow::seedTimelineEvents()
{
  if (timelinePanel_ == nullptr) {
    return;
  }

  timeline::TimelineEvent startup_event;
  startup_event.timestamp = QDateTime::currentDateTimeUtc();
  startup_event.direction = timeline::TimelineDirection::Internal;
  startup_event.kind = timeline::TimelineEventKind::Lifecycle;
  startup_event.status = timeline::TimelineStatus::Success;
  startup_event.method = QStringLiteral("app/startup");
  startup_event.summary = tr("Timeline panel initialized");
  startup_event.payload = QJsonObject{
      {QStringLiteral("component"), QStringLiteral("TimelinePanel")},
      {QStringLiteral("filters"), QStringLiteral("method/status/direction")},
  };
  timelinePanel_->append_event(startup_event);
  timelinePanel_->append_event(timeline::stderr_line_event(tr("stderr log lines will appear in the timeline.")));
}

void MainWindow::seedToolPanel()
{
  if (toolsPanel_ == nullptr) {
    return;
  }

  demoTools_.clear();
  toolsPanel_->clear_tools();
  connect(toolsPanel_, &ui::ToolPanel::toolCallRequested, this, &MainWindow::sendToolsCallRequest);
}

void MainWindow::seedResourcePanel()
{
  if (resourcesPanel_ == nullptr) {
    return;
  }

  demoResources_.clear();
  resourcesPanel_->clear_resources();
  connect(resourcesPanel_, &ui::ResourcePanel::resourceReadRequested, this, &MainWindow::sendResourcesReadRequest);
}

void MainWindow::seedPromptPanel()
{
  if (promptsPanel_ == nullptr) {
    return;
  }

  demoPrompts_.clear();
  promptsPanel_->clear_prompts();
  connect(promptsPanel_, &ui::PromptPanel::promptGetRequested, this, &MainWindow::sendPromptsGetRequest);
}

void MainWindow::seedSecurityPanel()
{
  refreshSecurityScan();
}

void MainWindow::seedComparePanel()
{
  refreshCompareDiff();
}

void MainWindow::connectSelectedProfile()
{
  if (profileEditor_ == nullptr || connection_ == nullptr) {
    return;
  }

  const auto profile = profileEditor_->current_profile();
  if (!profile.has_value()) {
    statusBar()->showMessage(tr("Select a profile before connecting."), 3000);
    return;
  }

  const auto validation = profileEditor_->validation_result();
  if (validation.has_errors()) {
    statusBar()->showMessage(tr("Profile has validation errors: %1").arg(validation.summary()), 5000);
    return;
  }

  if (currentSnapshot_.has_value()) {
    previousSnapshot_ = currentSnapshot_;
  }
  clearLiveServerSurface();
  pendingRequests_.clear();
  initializeRequestId_.reset();
  serverDisplayName_ = profile->name;
  refreshSecurityScan();
  refreshCompareDiff();
  if (!connection_->connect_to(*profile)) {
    statusBar()->showMessage(tr("Failed to start transport."), 5000);
    updateConnectionActions();
    return;
  }

  statusBar()->showMessage(tr("Connecting profile: %1").arg(profile->name), 3000);
}

void MainWindow::disconnectSelectedProfile()
{
  if (connection_ == nullptr) {
    return;
  }

  pendingRequests_.clear();
  initializeRequestId_.reset();
  connection_->disconnect();
  updateConnectionActions();
}

void MainWindow::sendInitializeRequest()
{
  if (connection_ == nullptr || !connection_->is_connected()) {
    statusBar()->showMessage(tr("Connect to a running MCP server before initializing."), 3000);
    return;
  }

  if (initializeRequestId_.has_value()) {
    statusBar()->showMessage(tr("Initialize request is already pending."), 3000);
    return;
  }

  McpInitializeParams params;
  params.protocol_version = mcp::default_protocol_version();
  if (const auto& profile = connection_->profile(); profile.has_value() && !profile->protocol_version.trimmed().isEmpty()) {
    params.protocol_version = mcp::McpProtocolVersion{profile->protocol_version.trimmed()};
  }
  params.client_info = mcp::default_client_info(app::application_metadata().application_version);
  params.capabilities = QJsonObject{};

  const auto request = mcp::make_initialize_request(pendingRequests_, params);
  initializeRequestId_ = request.id;
  connection_->send(mcp::to_json_object(request));

  if (timelinePanel_ != nullptr) {
    timeline::TimelineEvent event;
    event.event_id = timeline::make_timeline_event_id();
    event.timestamp = QDateTime::currentDateTimeUtc();
    event.direction = timeline::TimelineDirection::Outbound;
    event.kind = timeline::TimelineEventKind::Request;
    event.status = timeline::TimelineStatus::Pending;
    event.request_id = request.id;
    event.method = request.method;
    event.summary = tr("Sent MCP initialize request");
    event.payload = mcp::to_json_object(request);
    timelinePanel_->append_event(std::move(event));
  }

  statusBar()->showMessage(tr("Initialize request sent."), 3000);
  updateConnectionActions();
}

void MainWindow::sendToolsListRequest(std::optional<QString> cursor)
{
  if (connection_ == nullptr || !connection_->is_connected()) {
    statusBar()->showMessage(tr("Connect and initialize a running MCP server before listing tools."), 3000);
    return;
  }

  const auto request = mcp::make_tools_list_request(pendingRequests_, std::move(cursor));
  connection_->send(mcp::to_json_object(request));

  if (timelinePanel_ != nullptr) {
    timeline::TimelineEvent event;
    event.event_id = timeline::make_timeline_event_id();
    event.timestamp = QDateTime::currentDateTimeUtc();
    event.direction = timeline::TimelineDirection::Outbound;
    event.kind = timeline::TimelineEventKind::Request;
    event.status = timeline::TimelineStatus::Pending;
    event.request_id = request.id;
    event.method = request.method;
    event.summary = tr("Sent MCP tools/list request");
    event.payload = mcp::to_json_object(request);
    timelinePanel_->append_event(std::move(event));
  }

  statusBar()->showMessage(tr("tools/list request sent."), 3000);
}

void MainWindow::sendToolsCallRequest(const QString& tool_name, const QJsonObject& arguments)
{
  if (connection_ == nullptr || !connection_->is_connected()) {
    if (toolsPanel_ != nullptr) {
      toolsPanel_->set_call_error(tr("Connect and initialize a running MCP server before running tools/call."));
    }
    statusBar()->showMessage(tr("Connect and initialize before running a tool."), 3000);
    return;
  }

  const auto request = mcp::make_tools_call_request(pendingRequests_, tool_name, arguments);
  connection_->send(mcp::to_json_object(request));

  if (timelinePanel_ != nullptr) {
    timeline::TimelineEvent event;
    event.event_id = timeline::make_timeline_event_id();
    event.timestamp = QDateTime::currentDateTimeUtc();
    event.direction = timeline::TimelineDirection::Outbound;
    event.kind = timeline::TimelineEventKind::Request;
    event.status = timeline::TimelineStatus::Pending;
    event.request_id = request.id;
    event.method = request.method;
    event.summary = tr("Sent MCP tools/call request: %1").arg(tool_name);
    event.payload = mcp::to_json_object(request);
    timelinePanel_->append_event(std::move(event));
  }

  statusBar()->showMessage(tr("tools/call request sent: %1").arg(tool_name), 3000);
}

void MainWindow::sendResourcesListRequest(std::optional<QString> cursor)
{
  if (connection_ == nullptr || !connection_->is_connected()) {
    statusBar()->showMessage(tr("Connect and initialize a running MCP server before listing resources."), 3000);
    return;
  }

  const auto request = mcp::make_resources_list_request(pendingRequests_, std::move(cursor));
  connection_->send(mcp::to_json_object(request));

  if (timelinePanel_ != nullptr) {
    timeline::TimelineEvent event;
    event.event_id = timeline::make_timeline_event_id();
    event.timestamp = QDateTime::currentDateTimeUtc();
    event.direction = timeline::TimelineDirection::Outbound;
    event.kind = timeline::TimelineEventKind::Request;
    event.status = timeline::TimelineStatus::Pending;
    event.request_id = request.id;
    event.method = request.method;
    event.summary = tr("Sent MCP resources/list request");
    event.payload = mcp::to_json_object(request);
    timelinePanel_->append_event(std::move(event));
  }

  statusBar()->showMessage(tr("resources/list request sent."), 3000);
}

void MainWindow::sendResourcesReadRequest(const QString& uri)
{
  if (connection_ == nullptr || !connection_->is_connected()) {
    if (resourcesPanel_ != nullptr) {
      resourcesPanel_->set_read_error(tr("Connect and initialize a running MCP server before running resources/read."));
    }
    statusBar()->showMessage(tr("Connect and initialize before reading a resource."), 3000);
    return;
  }

  const auto request = mcp::make_resources_read_request(pendingRequests_, uri);
  connection_->send(mcp::to_json_object(request));

  if (timelinePanel_ != nullptr) {
    timeline::TimelineEvent event;
    event.event_id = timeline::make_timeline_event_id();
    event.timestamp = QDateTime::currentDateTimeUtc();
    event.direction = timeline::TimelineDirection::Outbound;
    event.kind = timeline::TimelineEventKind::Request;
    event.status = timeline::TimelineStatus::Pending;
    event.request_id = request.id;
    event.method = request.method;
    event.summary = tr("Sent MCP resources/read request: %1").arg(uri);
    event.payload = mcp::to_json_object(request);
    timelinePanel_->append_event(std::move(event));
  }

  statusBar()->showMessage(tr("resources/read request sent."), 3000);
}

void MainWindow::sendPromptsListRequest(std::optional<QString> cursor)
{
  if (connection_ == nullptr || !connection_->is_connected()) {
    statusBar()->showMessage(tr("Connect and initialize a running MCP server before listing prompts."), 3000);
    return;
  }

  const auto request = mcp::make_prompts_list_request(pendingRequests_, std::move(cursor));
  connection_->send(mcp::to_json_object(request));

  if (timelinePanel_ != nullptr) {
    timeline::TimelineEvent event;
    event.event_id = timeline::make_timeline_event_id();
    event.timestamp = QDateTime::currentDateTimeUtc();
    event.direction = timeline::TimelineDirection::Outbound;
    event.kind = timeline::TimelineEventKind::Request;
    event.status = timeline::TimelineStatus::Pending;
    event.request_id = request.id;
    event.method = request.method;
    event.summary = tr("Sent MCP prompts/list request");
    event.payload = mcp::to_json_object(request);
    timelinePanel_->append_event(std::move(event));
  }

  statusBar()->showMessage(tr("prompts/list request sent."), 3000);
}

void MainWindow::sendPromptsGetRequest(const QString& prompt_name, const QJsonObject& arguments)
{
  if (connection_ == nullptr || !connection_->is_connected()) {
    if (promptsPanel_ != nullptr) {
      promptsPanel_->set_get_error(tr("Connect and initialize a running MCP server before running prompts/get."));
    }
    statusBar()->showMessage(tr("Connect and initialize before getting a prompt."), 3000);
    return;
  }

  const auto request = mcp::make_prompts_get_request(pendingRequests_, prompt_name, arguments);
  connection_->send(mcp::to_json_object(request));

  if (timelinePanel_ != nullptr) {
    timeline::TimelineEvent event;
    event.event_id = timeline::make_timeline_event_id();
    event.timestamp = QDateTime::currentDateTimeUtc();
    event.direction = timeline::TimelineDirection::Outbound;
    event.kind = timeline::TimelineEventKind::Request;
    event.status = timeline::TimelineStatus::Pending;
    event.request_id = request.id;
    event.method = request.method;
    event.summary = tr("Sent MCP prompts/get request: %1").arg(prompt_name);
    event.payload = mcp::to_json_object(request);
    timelinePanel_->append_event(std::move(event));
  }

  statusBar()->showMessage(tr("prompts/get request sent: %1").arg(prompt_name), 3000);
}

void MainWindow::runProfileLaunchTest(const config::ServerProfile& profile)
{
  QProcess process;
  if (!profile.cwd.trimmed().isEmpty()) {
    process.setWorkingDirectory(profile.cwd.trimmed());
  }

  auto environment = QProcessEnvironment::systemEnvironment();
  for (auto it = profile.env.cbegin(); it != profile.env.cend(); ++it) {
    environment.insert(it.key(), it.value());
  }
  process.setProcessEnvironment(environment);

  process.start(profile.command, profile.args);
  const auto timeout_value = profile.timeout_ms.value_or(3000);
  const auto timeout = static_cast<int>(std::clamp<qint64>(timeout_value, 0, 60000));
  if (!process.waitForStarted(timeout)) {
    statusBar()->showMessage(tr("Launch test failed: %1").arg(process.errorString()), 5000);
    return;
  }

  process.kill();
  process.waitForFinished(1000);
  statusBar()->showMessage(tr("Launch test succeeded; process started and was stopped."), 5000);
}

void MainWindow::handleIncomingMessage(const QJsonObject& message)
{
  if (timelinePanel_ != nullptr) {
    timeline::TimelineEvent event;
    event.timestamp = QDateTime::currentDateTimeUtc();
    event.direction = timeline::TimelineDirection::Inbound;
    event.kind = message.contains(QStringLiteral("method"))
        ? timeline::TimelineEventKind::Notification
        : timeline::TimelineEventKind::Response;
    event.status = message.contains(QStringLiteral("error"))
        ? timeline::TimelineStatus::Error
        : (message.contains(QStringLiteral("method")) ? timeline::TimelineStatus::Notification : timeline::TimelineStatus::Success);
    event.method = message.value(QStringLiteral("method")).toString(QStringLiteral("response"));
    event.summary = tr("Received MCP message: %1").arg(event.method);
    event.payload = message;
    timelinePanel_->append_event(std::move(event));
  }

  const auto parsed = mcp::parse_json_rpc_message(message);
  const auto* parsed_message = std::get_if<mcp::JsonRpcMessage>(&parsed);
  if (parsed_message == nullptr) {
    return;
  }

  const auto* response = std::get_if<mcp::JsonRpcResponse>(parsed_message);
  if (response == nullptr) {
    return;
  }

  const auto pending_request = pendingRequests_.complete(response->id);
  if (!pending_request.has_value()) {
    return;
  }

  if (pending_request->method == QString::fromUtf8(mcp::kMcpInitializeMethod)) {
    initializeRequestId_.reset();
    handleInitializeResponse(*response);
    updateConnectionActions();
    return;
  }

  if (pending_request->method == QString::fromUtf8(mcp::kMcpToolsListMethod)) {
    handleToolsListResponse(*response);
    return;
  }

  if (pending_request->method == QString::fromUtf8(mcp::kMcpToolsCallMethod)) {
    handleToolsCallResponse(*response);
    return;
  }

  if (pending_request->method == QString::fromUtf8(mcp::kMcpResourcesListMethod)) {
    handleResourcesListResponse(*response);
    return;
  }

  if (pending_request->method == QString::fromUtf8(mcp::kMcpResourcesReadMethod)) {
    handleResourcesReadResponse(*response);
    return;
  }

  if (pending_request->method == QString::fromUtf8(mcp::kMcpPromptsListMethod)) {
    handlePromptsListResponse(*response);
    return;
  }

  if (pending_request->method == QString::fromUtf8(mcp::kMcpPromptsGetMethod)) {
    handlePromptsGetResponse(*response);
  }
}

void MainWindow::handleInitializeResponse(const mcp::JsonRpcResponse& response)
{
  const auto parsed = mcp::parse_initialize_response(response);
  if (const auto* error = std::get_if<mcp::McpInitializeParseError>(&parsed)) {
    if (timelinePanel_ != nullptr) {
      timeline::TimelineEvent event;
      event.event_id = timeline::make_timeline_event_id();
      event.timestamp = QDateTime::currentDateTimeUtc();
      event.direction = timeline::TimelineDirection::Internal;
      event.kind = timeline::TimelineEventKind::Lifecycle;
      event.status = timeline::TimelineStatus::Error;
      event.method = QString::fromUtf8(mcp::kMcpInitializeMethod);
      event.summary = tr("MCP initialize failed: %1").arg(error->message);
      event.payload = QJsonObject{{QStringLiteral("error"), error->message}, {QStringLiteral("response"), mcp::to_json_object(response)}};
      timelinePanel_->append_event(std::move(event));
    }

    statusBar()->showMessage(tr("Initialize failed: %1").arg(error->message), 5000);
    return;
  }

  const auto& result = std::get<mcp::McpInitializeResult>(parsed);
  serverCapabilities_ = result.capabilities;
  serverDisplayName_ = QStringLiteral("%1 %2").arg(result.server_info.name, result.server_info.version);

  const auto initialized = mcp::make_initialized_notification();
  if (connection_ != nullptr && connection_->is_connected()) {
    connection_->send(mcp::to_json_object(initialized));
  }

  if (timelinePanel_ != nullptr) {
    timeline::TimelineEvent event;
    event.event_id = timeline::make_timeline_event_id();
    event.timestamp = QDateTime::currentDateTimeUtc();
    event.direction = timeline::TimelineDirection::Outbound;
    event.kind = timeline::TimelineEventKind::Notification;
    event.status = timeline::TimelineStatus::Notification;
    event.method = initialized.method;
    event.summary = tr("Sent MCP initialized notification");
    event.payload = mcp::to_json_object(initialized);
    timelinePanel_->append_event(std::move(event));
  }

  statusBar()->showMessage(
      tr("Initialized MCP server: %1 %2 (%3)").arg(result.server_info.name, result.server_info.version, result.protocol_version.value),
      5000);

  refreshCompareDiff();
  sendToolsListRequest();
  sendResourcesListRequest();
  sendPromptsListRequest();
}

void MainWindow::handleToolsListResponse(const mcp::JsonRpcResponse& response)
{
  const auto parsed = mcp::parse_tools_list_response(response);
  if (const auto* error = std::get_if<mcp::McpToolParseError>(&parsed)) {
    if (toolsPanel_ != nullptr) {
      toolsPanel_->set_call_error(tr("tools/list failed: %1").arg(error->message));
    }
    statusBar()->showMessage(tr("tools/list failed: %1").arg(error->message), 5000);
    return;
  }

  const auto& result = std::get<mcp::McpToolsListResult>(parsed);
  demoTools_ = result.tools;
  if (toolsPanel_ != nullptr) {
    toolsPanel_->set_tools(demoTools_);
    toolsPanel_->set_call_result(QJsonObject{{QStringLiteral("status"), QStringLiteral("tools-loaded")}, {QStringLiteral("method"), QString::fromUtf8(mcp::kMcpToolsListMethod)}, {QStringLiteral("result"), mcp::to_json_object(result)}});
  }

  refreshSecurityScan();
  refreshCompareDiff();
  statusBar()->showMessage(tr("Loaded %1 tool(s).").arg(static_cast<int>(result.tools.size())), 5000);
}

void MainWindow::handleToolsCallResponse(const mcp::JsonRpcResponse& response)
{
  const auto parsed = mcp::parse_tools_call_response(response);
  if (const auto* error = std::get_if<mcp::McpToolParseError>(&parsed)) {
    if (toolsPanel_ != nullptr) {
      toolsPanel_->set_call_error(error->message);
    }
    statusBar()->showMessage(tr("tools/call failed: %1").arg(error->message), 5000);
    return;
  }

  const auto& result = std::get<mcp::McpToolCallResult>(parsed);
  if (toolsPanel_ != nullptr) {
    toolsPanel_->set_call_result(result.raw_result.isEmpty() ? mcp::to_json_object(result) : result.raw_result);
  }

  statusBar()->showMessage(result.is_error ? tr("tools/call returned an error result.") : tr("tools/call completed."), 5000);
}

void MainWindow::handleResourcesListResponse(const mcp::JsonRpcResponse& response)
{
  const auto parsed = mcp::parse_resources_list_response(response);
  if (const auto* error = std::get_if<mcp::McpResourceParseError>(&parsed)) {
    if (resourcesPanel_ != nullptr) {
      resourcesPanel_->set_read_error(tr("resources/list failed: %1").arg(error->message));
    }
    statusBar()->showMessage(tr("resources/list failed: %1").arg(error->message), 5000);
    return;
  }

  const auto& result = std::get<mcp::McpResourcesListResult>(parsed);
  demoResources_ = result.resources;
  if (resourcesPanel_ != nullptr) {
    resourcesPanel_->set_resources(demoResources_);
    resourcesPanel_->set_read_result(QJsonObject{{QStringLiteral("status"), QStringLiteral("resources-loaded")}, {QStringLiteral("method"), QString::fromUtf8(mcp::kMcpResourcesListMethod)}, {QStringLiteral("result"), mcp::to_json_object(result)}});
  }

  refreshSecurityScan();
  refreshCompareDiff();
  statusBar()->showMessage(tr("Loaded %1 resource(s).").arg(static_cast<int>(result.resources.size())), 5000);
}

void MainWindow::handleResourcesReadResponse(const mcp::JsonRpcResponse& response)
{
  const auto parsed = mcp::parse_resources_read_response(response);
  if (const auto* error = std::get_if<mcp::McpResourceParseError>(&parsed)) {
    if (resourcesPanel_ != nullptr) {
      resourcesPanel_->set_read_error(error->message);
    }
    statusBar()->showMessage(tr("resources/read failed: %1").arg(error->message), 5000);
    return;
  }

  const auto& result = std::get<mcp::McpResourceReadResult>(parsed);
  if (resourcesPanel_ != nullptr) {
    resourcesPanel_->set_read_result(result.raw_result.isEmpty() ? mcp::to_json_object(result) : result.raw_result);
  }

  statusBar()->showMessage(tr("resources/read completed."), 5000);
}

void MainWindow::handlePromptsListResponse(const mcp::JsonRpcResponse& response)
{
  const auto parsed = mcp::parse_prompts_list_response(response);
  if (const auto* error = std::get_if<mcp::McpPromptParseError>(&parsed)) {
    if (promptsPanel_ != nullptr) {
      promptsPanel_->set_get_error(tr("prompts/list failed: %1").arg(error->message));
    }
    statusBar()->showMessage(tr("prompts/list failed: %1").arg(error->message), 5000);
    return;
  }

  const auto& result = std::get<mcp::McpPromptsListResult>(parsed);
  demoPrompts_ = result.prompts;
  if (promptsPanel_ != nullptr) {
    promptsPanel_->set_prompts(demoPrompts_);
    promptsPanel_->set_get_result(QJsonObject{{QStringLiteral("status"), QStringLiteral("prompts-loaded")}, {QStringLiteral("method"), QString::fromUtf8(mcp::kMcpPromptsListMethod)}, {QStringLiteral("result"), mcp::to_json_object(result)}});
  }

  refreshCompareDiff();
  statusBar()->showMessage(tr("Loaded %1 prompt(s).").arg(static_cast<int>(result.prompts.size())), 5000);
}

void MainWindow::handlePromptsGetResponse(const mcp::JsonRpcResponse& response)
{
  const auto parsed = mcp::parse_prompts_get_response(response);
  if (const auto* error = std::get_if<mcp::McpPromptParseError>(&parsed)) {
    if (promptsPanel_ != nullptr) {
      promptsPanel_->set_get_error(error->message);
    }
    statusBar()->showMessage(tr("prompts/get failed: %1").arg(error->message), 5000);
    return;
  }

  const auto& result = std::get<mcp::McpPromptGetResult>(parsed);
  if (promptsPanel_ != nullptr) {
    promptsPanel_->set_get_result(result.raw_result.isEmpty() ? mcp::to_json_object(result) : result.raw_result);
  }

  statusBar()->showMessage(tr("prompts/get completed."), 5000);
}

void MainWindow::refreshSecurityScan()
{
  if (securityPanel_ == nullptr) {
    return;
  }

  security::SecurityScanInput input;
  if (profileEditor_ != nullptr) {
    input.profile = profileEditor_->current_profile();
  }
  input.tools = demoTools_;
  input.resources = demoResources_;

  const auto result = securityScanner_.scan(input);
  securityPanel_->set_scan_result(result);
}

void MainWindow::refreshCompareDiff()
{
  if (comparePanel_ == nullptr) {
    return;
  }

  const auto after = currentServerSnapshot();
  currentSnapshot_ = after;

  compare::ServerSnapshot before;
  if (previousSnapshot_.has_value()) {
    before = *previousSnapshot_;
  } else {
    before.name = tr("No previous live snapshot");
    before.capabilities = QJsonObject{};
  }

  comparePanel_->set_diff_result(compare::diff_server_snapshots(before, after));
}

void MainWindow::exportSecurityReport()
{
  if (securityPanel_ == nullptr) {
    return;
  }

  const auto path = QFileDialog::getSaveFileName(
      this,
      tr("Export security report"),
      QStringLiteral("security-report.md"),
      tr("Markdown files (*.md);;HTML files (*.html *.htm);;All files (*)"));
  if (path.isEmpty()) {
    return;
  }

  QString profile_name;
  if (profileEditor_ != nullptr) {
    const auto profile = profileEditor_->current_profile();
    if (profile.has_value()) {
      profile_name = profile->name;
    }
  }

  std::vector<timeline::TimelineEvent> timeline_events;
  if (timelinePanel_ != nullptr) {
    timeline_events = timelinePanel_->store().events();
  }

  const auto model = report::make_report_model(
      securityPanel_->scan_result(),
      timeline_events,
      tr("Desktop MCP Inspector Security Report"),
      profile_name);
  const auto content = path.endsWith(QStringLiteral(".html"), Qt::CaseInsensitive)
          || path.endsWith(QStringLiteral(".htm"), Qt::CaseInsensitive)
      ? report::HTMLReportWriter{}.write(model)
      : report::MarkdownReportWriter{}.write(model);

  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
    QMessageBox::warning(this, tr("Report export failed"), file.errorString());
    return;
  }

  const auto bytes = content.toUtf8();
  if (file.write(bytes) != bytes.size()) {
    QMessageBox::warning(this, tr("Report export failed"), tr("Could not write the complete report file."));
    return;
  }

  statusBar()->showMessage(tr("Report exported: %1").arg(path), 5000);
}

void MainWindow::exportDiffReport()
{
  if (comparePanel_ == nullptr) {
    return;
  }

  const auto path = QFileDialog::getSaveFileName(
      this,
      tr("Export diff report"),
      QStringLiteral("diff-report.md"),
      tr("Markdown files (*.md);;All files (*)"));
  if (path.isEmpty()) {
    return;
  }

  const auto content = report::DiffMarkdownWriter{}.write(comparePanel_->diff_result());

  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
    QMessageBox::warning(this, tr("Diff report export failed"), file.errorString());
    return;
  }

  const auto bytes = content.toUtf8();
  if (file.write(bytes) != bytes.size()) {
    QMessageBox::warning(this, tr("Diff report export failed"), tr("Could not write the complete diff report file."));
    return;
  }

  statusBar()->showMessage(tr("Diff report exported: %1").arg(path), 5000);
}

void MainWindow::updateConnectionActions()
{
  const auto connected = connection_ != nullptr && connection_->is_connected();
  if (connectAction_ != nullptr) {
    connectAction_->setEnabled(!connected);
  }
  if (initializeAction_ != nullptr) {
    initializeAction_->setEnabled(connected && !initializeRequestId_.has_value());
  }
  if (disconnectAction_ != nullptr) {
    disconnectAction_->setEnabled(connected);
  }
}

void MainWindow::clearLiveServerSurface()
{
  serverCapabilities_ = QJsonObject{};
  serverDisplayName_.clear();
  demoTools_.clear();
  demoResources_.clear();
  demoPrompts_.clear();
  currentSnapshot_.reset();

  if (toolsPanel_ != nullptr) {
    toolsPanel_->clear_tools();
  }
  if (resourcesPanel_ != nullptr) {
    resourcesPanel_->clear_resources();
  }
  if (promptsPanel_ != nullptr) {
    promptsPanel_->clear_prompts();
  }
}

compare::ServerSnapshot MainWindow::currentServerSnapshot() const
{
  compare::ServerSnapshot snapshot;
  snapshot.name = serverDisplayName_;
  if (snapshot.name.trimmed().isEmpty() && profileEditor_ != nullptr) {
    const auto profile = profileEditor_->current_profile();
    if (profile.has_value()) {
      snapshot.name = profile->name;
    }
  }
  if (snapshot.name.trimmed().isEmpty()) {
    snapshot.name = tr("Current live server surface");
  }

  snapshot.capabilities = serverCapabilities_;
  snapshot.tools = demoTools_;
  snapshot.resources = demoResources_;
  snapshot.prompts = demoPrompts_;
  return snapshot;
}