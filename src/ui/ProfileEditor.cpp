#include "ProfileEditor.hpp"

#include "McpJson.hpp"
#include "ProfileDiff.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace ui {
namespace {

[[nodiscard]] bool is_secret_env_key(const QString& key)
{
  const auto lowered = key.toLower();
  return lowered.contains(QStringLiteral("secret")) || lowered.contains(QStringLiteral("token"))
      || lowered.contains(QStringLiteral("password")) || lowered.contains(QStringLiteral("passwd"))
      || lowered.contains(QStringLiteral("credential")) || lowered.endsWith(QStringLiteral("_key"));
}

[[nodiscard]] QString mask_value(const QString& key, const QString& value, bool mask_enabled)
{
  if (!mask_enabled || value.isEmpty() || !is_secret_env_key(key)) {
    return value;
  }

  return QStringLiteral("••••••");
}

[[nodiscard]] QString quote_argument(const QString& argument)
{
  if (argument.isEmpty()) {
    return QStringLiteral("\"\"");
  }

  if (!argument.contains(QChar::Space) && !argument.contains(QChar('"'))) {
    return argument;
  }

  auto escaped = argument;
  escaped.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
  escaped.replace(QStringLiteral("\""), QStringLiteral("\\\""));
  return QStringLiteral("\"%1\"").arg(escaped);
}

[[nodiscard]] QString join_arguments(const QStringList& arguments)
{
  QStringList quoted;
  for (const auto& argument : arguments) {
    quoted.push_back(quote_argument(argument));
  }

  return quoted.join(QChar::Space);
}

[[nodiscard]] int protocol_index(QComboBox* combo, const QString& protocol_version)
{
  return combo->findText(protocol_version, Qt::MatchFixedString);
}

} // namespace

ProfileEditor::ProfileEditor(QWidget* parent)
  : QWidget(parent)
{
  setup_ui();
  connect_controls();
  update_env_preview();
  validate_current_profile();
  refresh_profile_diff();
}

void ProfileEditor::set_store(config::ProfileStore* store)
{
  store_ = store;
  reload_profiles();
}

void ProfileEditor::reload_profiles()
{
  const QSignalBlocker blocker(profile_combo_);
  profile_combo_->clear();

  if (store_ == nullptr) {
    start_new_profile();
    return;
  }

  const auto recent_id = store_->recent_profile_id();
  int recent_index = -1;
  for (const auto& profile : store_->profiles()) {
    const auto label = profile.name.trimmed().isEmpty() ? profile.command : profile.name;
    profile_combo_->addItem(label, profile.id);
    if (profile.id == recent_id) {
      recent_index = profile_combo_->count() - 1;
    }
  }

  if (profile_combo_->count() == 0) {
    start_new_profile();
    return;
  }

  profile_combo_->setCurrentIndex(recent_index >= 0 ? recent_index : 0);
  const auto id = selected_profile_id();
  const auto selected = store_->profile_by_id(id);
  if (selected.has_value()) {
    update_fields_from_profile(*selected);
  }
}

void ProfileEditor::set_current_profile(const config::ServerProfile& profile)
{
  update_fields_from_profile(profile);
  for (int index = 0; index < profile_combo_->count(); ++index) {
    if (profile_combo_->itemData(index).toString() == profile.id) {
      const QSignalBlocker blocker(profile_combo_);
      profile_combo_->setCurrentIndex(index);
      break;
    }
  }
}

std::optional<config::ServerProfile> ProfileEditor::current_profile() const
{
  config::ServerProfile profile;
  profile.id = current_profile_id_;
  profile.name = name_edit_->text().trimmed();
  profile.transport = config::ServerTransportKind::Stdio;
  profile.command = command_edit_->text().trimmed();
  profile.args = parse_argument_line();
  profile.cwd = cwd_edit_->text().trimmed();
  profile.env = parse_env_lines();
  profile.protocol_version = protocol_version_combo_->currentText().trimmed();
  if (profile.protocol_version.isEmpty()) {
    profile.protocol_version = config::default_mcp_protocol_version();
  }

  const auto timeout_text = timeout_edit_->text().trimmed();
  if (!timeout_text.isEmpty()) {
    bool ok = false;
    const auto timeout = timeout_text.toLongLong(&ok);
    profile.timeout_ms = ok ? timeout : -1;
  }

  return profile;
}

config::ProfileValidationResult ProfileEditor::validation_result() const
{
  const auto profile = current_profile();
  if (!profile.has_value()) {
    config::ProfileValidationResult result;
    result.issues.push_back(config::ProfileValidationIssue{
        config::ProfileValidationSeverity::Error,
        QStringLiteral("profile"),
        tr("Profile could not be read from the editor."),
    });
    return result;
  }

  return config::validate_profile(*profile);
}

void ProfileEditor::setup_ui()
{
  auto* root_layout = new QVBoxLayout(this);
  root_layout->setContentsMargins(0, 0, 0, 0);
  root_layout->setSpacing(8);

  auto* selector_layout = new QHBoxLayout();
  selector_layout->setContentsMargins(0, 0, 0, 0);

  selector_layout->addWidget(new QLabel(tr("Profile"), this));
  profile_combo_ = new QComboBox(this);
  profile_combo_->setObjectName(QStringLiteral("profileSelector"));
  selector_layout->addWidget(profile_combo_, 1);

  new_button_ = new QPushButton(tr("New"), this);
  new_button_->setObjectName(QStringLiteral("profileNewButton"));
  clone_button_ = new QPushButton(tr("Clone"), this);
  clone_button_->setObjectName(QStringLiteral("profileCloneButton"));
  delete_button_ = new QPushButton(tr("Delete"), this);
  delete_button_->setObjectName(QStringLiteral("profileDeleteButton"));
  import_button_ = new QPushButton(tr("Import mcp.json…"), this);
  import_button_->setObjectName(QStringLiteral("profileImportMcpJsonButton"));
  export_button_ = new QPushButton(tr("Export mcp.json…"), this);
  export_button_->setObjectName(QStringLiteral("profileExportMcpJsonButton"));
  selector_layout->addWidget(new_button_);
  selector_layout->addWidget(clone_button_);
  selector_layout->addWidget(delete_button_);
  selector_layout->addWidget(import_button_);
  selector_layout->addWidget(export_button_);
  root_layout->addLayout(selector_layout);

  auto* form_layout = new QFormLayout();
  form_layout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

  name_edit_ = new QLineEdit(this);
  name_edit_->setObjectName(QStringLiteral("profileName"));
  command_edit_ = new QLineEdit(this);
  command_edit_->setObjectName(QStringLiteral("profileCommand"));
  command_edit_->setPlaceholderText(tr("node, python, uvx, or an absolute server command"));
  args_edit_ = new QLineEdit(this);
  args_edit_->setObjectName(QStringLiteral("profileArgs"));
  args_edit_->setPlaceholderText(tr("Arguments, parsed with shell-like quoting"));
  cwd_edit_ = new QLineEdit(this);
  cwd_edit_->setObjectName(QStringLiteral("profileCwd"));
  timeout_edit_ = new QLineEdit(this);
  timeout_edit_->setObjectName(QStringLiteral("profileTimeout"));
  timeout_edit_->setPlaceholderText(tr("Optional start timeout in milliseconds"));
  protocol_version_combo_ = new QComboBox(this);
  protocol_version_combo_->setObjectName(QStringLiteral("profileProtocolVersion"));
  protocol_version_combo_->setEditable(true);
  protocol_version_combo_->addItem(config::default_mcp_protocol_version());
  protocol_version_combo_->addItem(QStringLiteral("2024-11-05"));
  protocol_version_combo_->addItem(QStringLiteral("2024-06-25"));

  auto* cwd_layout = new QHBoxLayout();
  cwd_layout->setContentsMargins(0, 0, 0, 0);
  cwd_layout->addWidget(cwd_edit_, 1);
  cwd_button_ = new QPushButton(tr("Browse…"), this);
  cwd_button_->setObjectName(QStringLiteral("profileCwdBrowseButton"));
  cwd_layout->addWidget(cwd_button_);

  form_layout->addRow(tr("Name"), name_edit_);
  form_layout->addRow(tr("Command"), command_edit_);
  form_layout->addRow(tr("Arguments"), args_edit_);
  form_layout->addRow(tr("Working directory"), cwd_layout);
  form_layout->addRow(tr("Timeout"), timeout_edit_);
  form_layout->addRow(tr("Protocol version"), protocol_version_combo_);
  root_layout->addLayout(form_layout);

  root_layout->addWidget(new QLabel(tr("Environment (KEY=value per line)"), this));
  env_editor_ = new QPlainTextEdit(this);
  env_editor_->setObjectName(QStringLiteral("profileEnv"));
  env_editor_->setPlaceholderText(tr("API_TOKEN=...\nDEBUG=1"));
  root_layout->addWidget(env_editor_, 1);

  mask_secrets_checkbox_ = new QCheckBox(tr("Mask secret-like environment values in the preview"), this);
  mask_secrets_checkbox_->setObjectName(QStringLiteral("profileMaskSecrets"));
  mask_secrets_checkbox_->setChecked(true);
  root_layout->addWidget(mask_secrets_checkbox_);

  env_preview_ = new QPlainTextEdit(this);
  env_preview_->setObjectName(QStringLiteral("profileEnvPreview"));
  env_preview_->setReadOnly(true);
  env_preview_->setMaximumBlockCount(200);
  root_layout->addWidget(env_preview_, 1);

  root_layout->addWidget(new QLabel(tr("Profile diff (saved profile vs editor)"), this));
  diff_preview_ = new QPlainTextEdit(this);
  diff_preview_->setObjectName(QStringLiteral("profileDiffPreview"));
  diff_preview_->setReadOnly(true);
  diff_preview_->setMaximumBlockCount(200);
  root_layout->addWidget(diff_preview_, 1);

  validation_label_ = new QLabel(this);
  validation_label_->setObjectName(QStringLiteral("profileValidationStatus"));
  validation_label_->setWordWrap(true);

  validate_button_ = new QPushButton(tr("Validate profile"), this);
  validate_button_->setObjectName(QStringLiteral("profileValidateButton"));
  save_button_ = new QPushButton(tr("Save profile"), this);
  save_button_->setObjectName(QStringLiteral("profileSaveButton"));
  launch_test_button_ = new QPushButton(tr("Test launch"), this);
  launch_test_button_->setObjectName(QStringLiteral("profileLaunchTestButton"));

  auto* actions_layout = new QHBoxLayout();
  actions_layout->setContentsMargins(0, 0, 0, 0);
  actions_layout->addWidget(validation_label_, 1);
  actions_layout->addWidget(validate_button_);
  actions_layout->addWidget(save_button_);
  actions_layout->addWidget(launch_test_button_);
  root_layout->addLayout(actions_layout);
}

void ProfileEditor::connect_controls()
{
  connect(profile_combo_, &QComboBox::currentIndexChanged, this, [this](int) {
    if (store_ == nullptr) {
      return;
    }

    const auto selected = store_->profile_by_id(selected_profile_id());
    if (selected.has_value()) {
      update_fields_from_profile(*selected);
      emit profileSelected(*selected);
    }
  });
  connect(new_button_, &QPushButton::clicked, this, &ProfileEditor::start_new_profile);
  connect(save_button_, &QPushButton::clicked, this, &ProfileEditor::save_current_profile);
  connect(clone_button_, &QPushButton::clicked, this, &ProfileEditor::clone_selected_profile);
  connect(delete_button_, &QPushButton::clicked, this, &ProfileEditor::delete_selected_profile);
  connect(import_button_, &QPushButton::clicked, this, &ProfileEditor::import_mcp_json);
  connect(export_button_, &QPushButton::clicked, this, &ProfileEditor::export_mcp_json);
  connect(validate_button_, &QPushButton::clicked, this, &ProfileEditor::validate_current_profile);
  connect(launch_test_button_, &QPushButton::clicked, this, &ProfileEditor::request_launch_test);
  connect(cwd_button_, &QPushButton::clicked, this, &ProfileEditor::browse_working_directory);
  connect(env_editor_, &QPlainTextEdit::textChanged, this, &ProfileEditor::update_env_preview);
  connect(mask_secrets_checkbox_, &QCheckBox::toggled, this, &ProfileEditor::update_env_preview);

  const auto refresh_after_edit = [this]() {
    validate_current_profile();
    refresh_profile_diff();
  };
  connect(name_edit_, &QLineEdit::textChanged, this, refresh_after_edit);
  connect(command_edit_, &QLineEdit::textChanged, this, refresh_after_edit);
  connect(args_edit_, &QLineEdit::textChanged, this, refresh_after_edit);
  connect(cwd_edit_, &QLineEdit::textChanged, this, refresh_after_edit);
  connect(timeout_edit_, &QLineEdit::textChanged, this, refresh_after_edit);
  connect(env_editor_, &QPlainTextEdit::textChanged, this, refresh_after_edit);
  connect(protocol_version_combo_, &QComboBox::currentTextChanged, this, refresh_after_edit);
}

void ProfileEditor::start_new_profile()
{
  current_profile_id_.clear();
  name_edit_->clear();
  command_edit_->clear();
  args_edit_->clear();
  cwd_edit_->clear();
  timeout_edit_->setText(QStringLiteral("3000"));
  const QSignalBlocker blocker(protocol_version_combo_);
  protocol_version_combo_->setCurrentText(config::default_mcp_protocol_version());
  env_editor_->clear();
  update_env_preview();
  validate_current_profile();
  refresh_profile_diff();
}

void ProfileEditor::save_current_profile()
{
  if (store_ == nullptr) {
    return;
  }

  const auto profile = current_profile();
  if (!profile.has_value()) {
    return;
  }

  const auto validation = config::validate_profile(*profile);
  show_validation_result(validation);
  if (validation.has_errors()) {
    return;
  }

  const auto saved = store_->upsert(*profile);
  store_->set_recent_profile_id(saved.id);

  QString error_message;
  if (!store_->save(&error_message)) {
    validation_label_->setText(tr("Failed to save profile: %1").arg(error_message));
    return;
  }

  reload_profiles();
  set_current_profile(saved);
  refresh_profile_diff();
  emit profileSaved(saved);
}

void ProfileEditor::clone_selected_profile()
{
  if (store_ == nullptr) {
    return;
  }

  const auto clone = store_->clone_profile(selected_profile_id());
  if (!clone.has_value()) {
    return;
  }

  store_->set_recent_profile_id(clone->id);
  QString error_message;
  store_->save(&error_message);
  reload_profiles();
  set_current_profile(*clone);
  emit profileCloned(*clone);
}

void ProfileEditor::delete_selected_profile()
{
  if (store_ == nullptr) {
    return;
  }

  const auto id = selected_profile_id();
  if (id.isEmpty()) {
    return;
  }

  if (!store_->remove(id)) {
    return;
  }

  QString error_message;
  store_->save(&error_message);
  reload_profiles();
  emit profileDeleted(id);
}

void ProfileEditor::validate_current_profile()
{
  show_validation_result(validation_result());
}

void ProfileEditor::request_launch_test()
{
  const auto profile = current_profile();
  if (!profile.has_value()) {
    return;
  }

  const auto validation = config::validate_profile(*profile);
  show_validation_result(validation);
  if (validation.has_errors()) {
    return;
  }

  emit launchTestRequested(*profile);
}

void ProfileEditor::browse_working_directory()
{
  const auto directory = QFileDialog::getExistingDirectory(this, tr("Select working directory"), cwd_edit_->text());
  if (!directory.isEmpty()) {
    cwd_edit_->setText(directory);
  }
}

void ProfileEditor::import_mcp_json()
{
  if (store_ == nullptr) {
    return;
  }

  const auto path = QFileDialog::getOpenFileName(
      this,
      tr("Import mcp.json"),
      QString(),
      tr("MCP config files (mcp.json *.json);;JSON files (*.json);;All files (*)"));
  if (path.isEmpty()) {
    return;
  }

  const auto result = config::load_mcp_json_file(path);
  show_validation_result(result.validation);
  if (result.validation.has_errors()) {
    QMessageBox::warning(this, tr("mcp.json import failed"), result.summary());
    return;
  }

  config::ServerProfile last_imported;
  for (const auto& profile : result.profiles) {
    last_imported = store_->upsert(profile);
  }
  if (!last_imported.id.isEmpty()) {
    store_->set_recent_profile_id(last_imported.id);
  }

  QString error_message;
  if (!store_->save(&error_message)) {
    QMessageBox::warning(this, tr("mcp.json import failed"), tr("Could not save imported profiles: %1").arg(error_message));
    return;
  }

  reload_profiles();
  if (!last_imported.id.isEmpty()) {
    set_current_profile(last_imported);
    emit profileSaved(last_imported);
  }
  QMessageBox::information(this, tr("mcp.json import"), tr("Imported %n profile(s).", nullptr, result.profiles.size()));
}

void ProfileEditor::export_mcp_json()
{
  QVector<config::ServerProfile> profiles;
  if (store_ != nullptr) {
    profiles = store_->profiles();
  }

  if (profiles.empty()) {
    const auto profile = current_profile();
    if (profile.has_value()) {
      profiles.push_back(*profile);
    }
  }

  const auto validation = config::validate_mcp_json_profiles(profiles);
  show_validation_result(validation);
  if (validation.has_errors()) {
    QMessageBox::warning(this, tr("mcp.json export failed"), validation.summary());
    return;
  }

  const auto path = QFileDialog::getSaveFileName(
      this,
      tr("Export mcp.json"),
      QStringLiteral("mcp.json"),
      tr("MCP config files (mcp.json *.json);;JSON files (*.json);;All files (*)"));
  if (path.isEmpty()) {
    return;
  }

  QString error_message;
  if (!config::save_mcp_json_file(path, profiles, &error_message)) {
    QMessageBox::warning(this, tr("mcp.json export failed"), error_message);
    return;
  }

  QMessageBox::information(this, tr("mcp.json export"), tr("Exported %n profile(s).", nullptr, profiles.size()));
}

void ProfileEditor::update_fields_from_profile(const config::ServerProfile& profile)
{
  current_profile_id_ = profile.id;
  name_edit_->setText(profile.name);
  command_edit_->setText(profile.command);
  args_edit_->setText(join_arguments(profile.args));
  cwd_edit_->setText(profile.cwd);
  timeout_edit_->setText(profile.timeout_ms.has_value() ? QString::number(*profile.timeout_ms) : QString());

  const QSignalBlocker blocker(protocol_version_combo_);
  const auto protocol = profile.protocol_version.trimmed().isEmpty()
      ? config::default_mcp_protocol_version()
      : profile.protocol_version.trimmed();
  if (protocol_index(protocol_version_combo_, protocol) < 0) {
    protocol_version_combo_->addItem(protocol);
  }
  protocol_version_combo_->setCurrentText(protocol);

  QStringList env_lines;
  for (auto it = profile.env.cbegin(); it != profile.env.cend(); ++it) {
    env_lines.push_back(QStringLiteral("%1=%2").arg(it.key(), it.value()));
  }
  env_editor_->setPlainText(env_lines.join(QStringLiteral("\n")));

  update_env_preview();
  validate_current_profile();
  refresh_profile_diff();
}

void ProfileEditor::update_env_preview()
{
  const auto env = parse_env_lines();
  QStringList lines;
  for (auto it = env.cbegin(); it != env.cend(); ++it) {
    lines.push_back(QStringLiteral("%1=%2").arg(
        it.key(),
        mask_value(it.key(), it.value(), mask_secrets_checkbox_->isChecked())));
  }

  env_preview_->setPlainText(lines.join(QStringLiteral("\n")));
}

void ProfileEditor::refresh_profile_diff()
{
  if (diff_preview_ == nullptr) {
    return;
  }

  const auto profile = current_profile();
  if (!profile.has_value()) {
    diff_preview_->clear();
    return;
  }

  std::optional<config::ServerProfile> saved_profile;
  if (store_ != nullptr && !current_profile_id_.isEmpty()) {
    saved_profile = store_->profile_by_id(current_profile_id_);
  }

  const auto diff = config::diff_profiles(saved_profile, *profile);
  diff_preview_->setPlainText(diff.summary());
}

void ProfileEditor::show_validation_result(const config::ProfileValidationResult& result)
{
  validation_label_->setText(result.summary());
}

QString ProfileEditor::selected_profile_id() const
{
  return profile_combo_->currentData().toString();
}

QMap<QString, QString> ProfileEditor::parse_env_lines() const
{
  QMap<QString, QString> env;
  const auto lines = env_editor_->toPlainText().split(QChar('\n'));
  for (const auto& raw_line : lines) {
    const auto line = raw_line.trimmed();
    if (line.isEmpty() || line.startsWith(QChar('#'))) {
      continue;
    }

    const auto separator = line.indexOf(QChar('='));
    if (separator < 0) {
      env.insert(line, QString());
      continue;
    }

    const auto key = line.left(separator).trimmed();
    const auto value = line.mid(separator + 1);
    env.insert(key, value);
  }

  return env;
}

QStringList ProfileEditor::parse_argument_line() const
{
  const auto text = args_edit_->text().trimmed();
  if (text.isEmpty()) {
    return {};
  }

  return QProcess::splitCommand(text);
}

} // namespace ui
