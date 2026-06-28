#pragma once

#include "ProfileStore.hpp"
#include "ProfileValidation.hpp"
#include "ServerProfile.hpp"

#include <QMap>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <optional>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

namespace ui {

class ProfileEditor final : public QWidget
{
  Q_OBJECT

public:
  explicit ProfileEditor(QWidget* parent = nullptr);

  void set_store(config::ProfileStore* store);
  void reload_profiles();
  void set_current_profile(const config::ServerProfile& profile);

  [[nodiscard]] std::optional<config::ServerProfile> current_profile() const;
  [[nodiscard]] config::ProfileValidationResult validation_result() const;

signals:
  void profileSaved(const config::ServerProfile& profile);
  void profileSelected(const config::ServerProfile& profile);
  void profileDeleted(const QString& profile_id);
  void profileCloned(const config::ServerProfile& profile);
  void launchTestRequested(const config::ServerProfile& profile);

private:
  void setup_ui();
  void connect_controls();

  void start_new_profile();
  void save_current_profile();
  void clone_selected_profile();
  void delete_selected_profile();
  void validate_current_profile();
  void request_launch_test();
  void browse_working_directory();
  void import_mcp_json();
  void export_mcp_json();

  void update_fields_from_profile(const config::ServerProfile& profile);
  void update_env_preview();
  void refresh_profile_diff();
  void show_validation_result(const config::ProfileValidationResult& result);

  [[nodiscard]] QString selected_profile_id() const;
  [[nodiscard]] QMap<QString, QString> parse_env_lines() const;
  [[nodiscard]] QStringList parse_argument_line() const;

  config::ProfileStore* store_ = nullptr;
  QString current_profile_id_;

  QComboBox* profile_combo_ = nullptr;
  QComboBox* protocol_version_combo_ = nullptr;
  QLineEdit* name_edit_ = nullptr;
  QLineEdit* command_edit_ = nullptr;
  QLineEdit* args_edit_ = nullptr;
  QLineEdit* cwd_edit_ = nullptr;
  QLineEdit* timeout_edit_ = nullptr;
  QPlainTextEdit* env_editor_ = nullptr;
  QPlainTextEdit* env_preview_ = nullptr;
  QPlainTextEdit* diff_preview_ = nullptr;
  QCheckBox* mask_secrets_checkbox_ = nullptr;
  QLabel* validation_label_ = nullptr;
  QPushButton* save_button_ = nullptr;
  QPushButton* new_button_ = nullptr;
  QPushButton* clone_button_ = nullptr;
  QPushButton* delete_button_ = nullptr;
  QPushButton* import_button_ = nullptr;
  QPushButton* export_button_ = nullptr;
  QPushButton* cwd_button_ = nullptr;
  QPushButton* validate_button_ = nullptr;
  QPushButton* launch_test_button_ = nullptr;
};

} // namespace ui
