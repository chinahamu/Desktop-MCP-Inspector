#pragma once

#include "ServerProfile.hpp"

#include <QString>
#include <QVector>

#include <optional>

namespace config {

class ProfileStore final
{
public:
  enum class Backend {
    Memory,
    Sqlite,
  };

  ProfileStore();

  [[nodiscard]] static ProfileStore in_memory();
  [[nodiscard]] static ProfileStore sqlite(QString database_path);
  [[nodiscard]] static QString default_sqlite_path();

  [[nodiscard]] Backend backend() const;
  [[nodiscard]] const QString& database_path() const;

  [[nodiscard]] const QVector<ServerProfile>& profiles() const;
  [[nodiscard]] bool is_empty() const;
  [[nodiscard]] std::optional<ServerProfile> profile_by_id(const QString& id) const;

  [[nodiscard]] QString recent_profile_id() const;
  void set_recent_profile_id(QString id);
  [[nodiscard]] std::optional<ServerProfile> recent_profile() const;

  ServerProfile upsert(ServerProfile profile);
  bool remove(const QString& id);
  std::optional<ServerProfile> clone_profile(const QString& id, const QString& name_suffix = QStringLiteral(" Copy"));
  void clear();

  bool load(QString* error_message = nullptr);
  bool save(QString* error_message = nullptr) const;

private:
  [[nodiscard]] int index_of(const QString& id) const;

  Backend backend_ = Backend::Memory;
  QString database_path_;
  QVector<ServerProfile> profiles_;
  QString recent_profile_id_;
};

} // namespace config
