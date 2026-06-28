#pragma once

#include <QString>

class QCoreApplication;
class QSettings;

namespace app {

struct ApplicationMetadata
{
  QString application_name;
  QString application_version;
  QString organization_name;
  QString organization_domain;
};

ApplicationMetadata application_metadata();
void apply_application_metadata(QCoreApplication& app);

QString settings_scope_path();
QString settings_file_path();
QSettings create_user_settings();

} // namespace app
