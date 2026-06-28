#include "AppSettings.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>

namespace app {
namespace {
constexpr auto kApplicationName = "Desktop MCP Inspector";
constexpr auto kApplicationVersion = "0.1.0";
constexpr auto kOrganizationName = "Meta Alchemist";
constexpr auto kOrganizationDomain = "meta-alchemist.co.jp";
constexpr auto kSettingsDirectoryName = "DesktopMCPInspector";
constexpr auto kSettingsFileName = "settings.ini";
} // namespace

ApplicationMetadata application_metadata()
{
  return {
      QString::fromUtf8(kApplicationName),
      QString::fromUtf8(kApplicationVersion),
      QString::fromUtf8(kOrganizationName),
      QString::fromUtf8(kOrganizationDomain),
  };
}

void apply_application_metadata(QCoreApplication& app)
{
  const auto metadata = application_metadata();

  app.setApplicationName(metadata.application_name);
  app.setApplicationVersion(metadata.application_version);
  app.setOrganizationName(metadata.organization_name);
  app.setOrganizationDomain(metadata.organization_domain);
}

QString settings_scope_path()
{
  const auto base_path = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
  if (!base_path.isEmpty()) {
    return base_path;
  }

  const auto fallback_path = QDir::homePath() + QStringLiteral("/.") + QString::fromUtf8(kSettingsDirectoryName);
  return QDir::cleanPath(fallback_path);
}

QString settings_file_path()
{
  QDir settings_dir(settings_scope_path());
  if (!settings_dir.exists()) {
    settings_dir.mkpath(QStringLiteral("."));
  }

  return settings_dir.filePath(QString::fromUtf8(kSettingsFileName));
}

QSettings create_user_settings()
{
  return {settings_file_path(), QSettings::IniFormat};
}

} // namespace app
