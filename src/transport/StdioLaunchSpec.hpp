#pragma once

#include <QMap>
#include <QString>
#include <QStringList>
#include <QtTypes>

#include <optional>

namespace config {
struct ServerProfile;
}

namespace transport {

struct StdioLaunchSpec
{
  QString program;
  QStringList arguments;
  QString working_directory;
  QMap<QString, QString> environment;
  std::optional<qint64> timeout_ms;

  friend bool operator==(const StdioLaunchSpec& lhs, const StdioLaunchSpec& rhs) = default;
};

[[nodiscard]] StdioLaunchSpec make_stdio_launch_spec(const config::ServerProfile& profile);

} // namespace transport
