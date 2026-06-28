#pragma once

#include <QJsonObject>
#include <QMetaType>
#include <QObject>
#include <QString>

#include <optional>

namespace config {

struct ServerProfile;

} // namespace config

namespace transport {

enum class TransportState {
  Idle,
  Starting,
  Running,
  Stopping,
  Stopped,
  Failed,
};

enum class TransportErrorCategory {
  StartFailed,
  WriteFailed,
  ReadFailed,
  ProtocolViolation,
  ProcessExited,
  Timeout,
  HttpError,
  JsonRpcError,
  Unknown,
};

struct TransportError
{
  TransportErrorCategory category = TransportErrorCategory::Unknown;
  QString message;
  std::optional<int> exit_code;
  std::optional<QJsonObject> details;

  friend bool operator==(const TransportError& lhs, const TransportError& rhs) = default;
};

class Transport : public QObject
{
  Q_OBJECT

public:
  explicit Transport(QObject* parent = nullptr);
  ~Transport() override;

  virtual void start(const config::ServerProfile& profile) = 0;
  virtual void stop() = 0;
  virtual void send(const QJsonObject& message) = 0;

  [[nodiscard]] virtual TransportState state() const = 0;
  [[nodiscard]] bool is_running() const;

signals:
  void messageReceived(const QJsonObject& message);
  void stderrReceived(const QString& text);
  void errorOccurred(const transport::TransportError& error);
  void stateChanged(transport::TransportState state);
};

[[nodiscard]] QString transport_state_name(TransportState state);
[[nodiscard]] QString transport_error_category_name(TransportErrorCategory category);

} // namespace transport

Q_DECLARE_METATYPE(transport::TransportState)
Q_DECLARE_METATYPE(transport::TransportError)
