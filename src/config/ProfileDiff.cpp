#include "ProfileDiff.hpp"

#include <QStringList>

#include <utility>

namespace config {
namespace {

[[nodiscard]] QString transport_label(ServerTransportKind kind)
{
  switch (kind) {
    case ServerTransportKind::Stdio:
      return QStringLiteral("stdio");
    case ServerTransportKind::StreamableHttp:
      return QStringLiteral("streamable-http");
  }

  return QStringLiteral("unknown");
}

[[nodiscard]] QString optional_timeout_label(const std::optional<qint64>& timeout)
{
  return timeout.has_value() ? QString::number(*timeout) : QStringLiteral("<default>");
}

[[nodiscard]] QString args_label(const QStringList& args)
{
  return args.isEmpty() ? QStringLiteral("<empty>") : args.join(QStringLiteral(" "));
}

[[nodiscard]] QString map_label(const QMap<QString, QString>& values)
{
  if (values.empty()) {
    return QStringLiteral("<empty>");
  }

  QStringList lines;
  for (auto it = values.cbegin(); it != values.cend(); ++it) {
    lines.push_back(QStringLiteral("%1=%2").arg(it.key(), it.value()));
  }

  return lines.join(QStringLiteral(", "));
}

void add_entry(ProfileDiff& diff, QString field, QString before, QString after, ProfileDiffKind kind)
{
  diff.entries.push_back(ProfileDiffEntry{
      std::move(field),
      std::move(before),
      std::move(after),
      kind,
  });
}

void add_if_changed(ProfileDiff& diff, const QString& field, const QString& before, const QString& after)
{
  if (before == after) {
    return;
  }

  add_entry(diff, field, before, after, ProfileDiffKind::Changed);
}

[[nodiscard]] QString profile_key(const ServerProfile& profile)
{
  if (!profile.name.trimmed().isEmpty()) {
    return QStringLiteral("name:%1").arg(profile.name.trimmed().toLower());
  }
  if (!profile.id.trimmed().isEmpty()) {
    return QStringLiteral("id:%1").arg(profile.id.trimmed());
  }

  return QStringLiteral("command:%1").arg(profile.command.trimmed().toLower());
}

[[nodiscard]] int find_profile_index(const QVector<ServerProfile>& profiles, const ServerProfile& profile)
{
  const auto key = profile_key(profile);
  for (int index = 0; index < profiles.size(); ++index) {
    if (profile_key(profiles.at(index)) == key) {
      return index;
    }
  }

  return -1;
}

} // namespace

bool ProfileDiff::has_changes() const
{
  for (const auto& entry : entries) {
    if (entry.kind != ProfileDiffKind::Unchanged) {
      return true;
    }
  }

  return false;
}

QString ProfileDiff::summary() const
{
  if (entries.empty()) {
    return QStringLiteral("No profile changes.");
  }

  QStringList lines;
  lines.push_back(QStringLiteral("Profile diff: %1").arg(profile_name.isEmpty() ? profile_id : profile_name));
  for (const auto& entry : entries) {
    switch (entry.kind) {
      case ProfileDiffKind::Added:
        lines.push_back(QStringLiteral("+ %1: %2").arg(entry.field, entry.after));
        break;
      case ProfileDiffKind::Removed:
        lines.push_back(QStringLiteral("- %1: %2").arg(entry.field, entry.before));
        break;
      case ProfileDiffKind::Changed:
        lines.push_back(QStringLiteral("~ %1: %2 -> %3").arg(entry.field, entry.before, entry.after));
        break;
      case ProfileDiffKind::Unchanged:
        break;
    }
  }

  return lines.join(QStringLiteral("\n"));
}

bool ProfileSetDiff::has_changes() const
{
  if (!added_profiles.empty() || !removed_profiles.empty()) {
    return true;
  }

  for (const auto& diff : changed_profiles) {
    if (diff.has_changes()) {
      return true;
    }
  }

  return false;
}

QString ProfileSetDiff::summary() const
{
  if (!has_changes()) {
    return QStringLiteral("No profile set changes.");
  }

  QStringList lines;
  for (const auto& profile : added_profiles) {
    lines.push_back(QStringLiteral("+ Profile added: %1").arg(profile.name));
  }
  for (const auto& profile : removed_profiles) {
    lines.push_back(QStringLiteral("- Profile removed: %1").arg(profile.name));
  }
  for (const auto& diff : changed_profiles) {
    if (diff.has_changes()) {
      lines.push_back(diff.summary());
    }
  }

  return lines.join(QStringLiteral("\n"));
}

ProfileDiff diff_profiles(const std::optional<ServerProfile>& before, const ServerProfile& after)
{
  ProfileDiff diff;
  diff.profile_id = after.id;
  diff.profile_name = after.name;

  if (!before.has_value()) {
    add_entry(diff, QStringLiteral("profile"), QString(), QStringLiteral("new"), ProfileDiffKind::Added);
    if (!after.name.isEmpty()) {
      add_entry(diff, QStringLiteral("name"), QString(), after.name, ProfileDiffKind::Added);
    }
    if (!after.command.isEmpty()) {
      add_entry(diff, QStringLiteral("command"), QString(), after.command, ProfileDiffKind::Added);
    }
    if (!after.endpoint_url.isEmpty()) {
      add_entry(diff, QStringLiteral("endpoint_url"), QString(), after.endpoint_url, ProfileDiffKind::Added);
    }
    return diff;
  }

  const auto& source = *before;
  add_if_changed(diff, QStringLiteral("name"), source.name, after.name);
  add_if_changed(diff, QStringLiteral("transport"), transport_label(source.transport), transport_label(after.transport));
  add_if_changed(diff, QStringLiteral("command"), source.command, after.command);
  add_if_changed(diff, QStringLiteral("args"), args_label(source.args), args_label(after.args));
  add_if_changed(diff, QStringLiteral("env"), map_label(source.env), map_label(after.env));
  add_if_changed(diff, QStringLiteral("cwd"), source.cwd, after.cwd);
  add_if_changed(diff, QStringLiteral("timeout_ms"), optional_timeout_label(source.timeout_ms), optional_timeout_label(after.timeout_ms));
  add_if_changed(diff, QStringLiteral("endpoint_url"), source.endpoint_url, after.endpoint_url);
  add_if_changed(diff, QStringLiteral("http_headers"), map_label(source.http_headers), map_label(after.http_headers));
  add_if_changed(diff, QStringLiteral("protocol_version"), source.protocol_version, after.protocol_version);

  return diff;
}

ProfileSetDiff diff_profile_sets(const QVector<ServerProfile>& before, const QVector<ServerProfile>& after)
{
  ProfileSetDiff result;
  QVector<int> matched_before_indexes;

  for (const auto& after_profile : after) {
    const auto before_index = find_profile_index(before, after_profile);
    if (before_index < 0) {
      result.added_profiles.push_back(after_profile);
      continue;
    }

    matched_before_indexes.push_back(before_index);
    auto diff = diff_profiles(std::optional<ServerProfile>{before.at(before_index)}, after_profile);
    if (diff.has_changes()) {
      result.changed_profiles.push_back(std::move(diff));
    }
  }

  for (int index = 0; index < before.size(); ++index) {
    if (!matched_before_indexes.contains(index)) {
      result.removed_profiles.push_back(before.at(index));
    }
  }

  return result;
}

} // namespace config
