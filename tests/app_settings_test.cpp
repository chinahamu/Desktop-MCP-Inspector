#include <catch2/catch_test_macros.hpp>

#include "AppSettings.hpp"

#include <QCoreApplication>
#include <QSettings>
#include <QString>

TEST_CASE("application metadata is centralized", "[app][settings]")
{
  const auto metadata = app::application_metadata();

  REQUIRE(metadata.application_name == QStringLiteral("Desktop MCP Inspector"));
  REQUIRE(metadata.application_version == QStringLiteral("0.1.0"));
  REQUIRE(metadata.organization_name == QStringLiteral("Meta Alchemist"));
  REQUIRE(metadata.organization_domain == QStringLiteral("meta-alchemist.co.jp"));
}

TEST_CASE("application metadata is applied to QCoreApplication", "[app][settings]")
{
  int argc = 0;
  QCoreApplication application(argc, nullptr);

  app::apply_application_metadata(application);

  REQUIRE(QCoreApplication::applicationName() == QStringLiteral("Desktop MCP Inspector"));
  REQUIRE(QCoreApplication::applicationVersion() == QStringLiteral("0.1.0"));
  REQUIRE(QCoreApplication::organizationName() == QStringLiteral("Meta Alchemist"));
  REQUIRE(QCoreApplication::organizationDomain() == QStringLiteral("meta-alchemist.co.jp"));
}

TEST_CASE("settings path resolves to ini file", "[app][settings]")
{
  const auto settings_path = app::settings_file_path();

  REQUIRE_FALSE(settings_path.isEmpty());
  REQUIRE(settings_path.endsWith(QStringLiteral("settings.ini")));

  auto settings = app::create_user_settings();
  REQUIRE(settings.format() == QSettings::IniFormat);
  REQUIRE(settings.fileName() == settings_path);
}
