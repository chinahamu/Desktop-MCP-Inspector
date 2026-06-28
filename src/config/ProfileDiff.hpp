#pragma once

#include "ServerProfile.hpp"

#include <QString>
#include <QVector>

#include <optional>

namespace config {

enum class ProfileDiffKind {
  Added,
  Removed,
  Changed,
  Unchanged,
};

struct ProfileDiffEntry
{
  QString field;
  QString before;
  QString after;
  ProfileDiffKind kind = ProfileDiffKind::Unchanged;

  friend bool operator==(const ProfileDiffEntry& lhs, const ProfileDiffEntry& rhs) = default;
};

struct ProfileDiff
{
  QString profile_id;
  QString profile_name;
  QVector<ProfileDiffEntry> entries;

  [[nodiscard]] bool has_changes() const;
  [[nodiscard]] QString summary() const;
};

struct ProfileSetDiff
{
  QVector<ProfileDiff> changed_profiles;
  QVector<ServerProfile> added_profiles;
  QVector<ServerProfile> removed_profiles;

  [[nodiscard]] bool has_changes() const;
  [[nodiscard]] QString summary() const;
};

[[nodiscard]] ProfileDiff diff_profiles(const std::optional<ServerProfile>& before, const ServerProfile& after);
[[nodiscard]] ProfileSetDiff diff_profile_sets(const QVector<ServerProfile>& before, const QVector<ServerProfile>& after);

} // namespace config
