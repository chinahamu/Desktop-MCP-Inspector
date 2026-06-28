#pragma once

#include "ServerProfile.hpp"
#include "Transport.hpp"

#include <QJsonObject>
#include <QObject>

#include <memory>
#include <optional>

namespace transport {

class TransportConnection final : public QObject
{
  Q_OBJECT

public:
  explicit TransportConnection(std::unique_ptr<Transport> transport, QObject* parent = nullptr);
  ~TransportConnection() override;

  TransportConnection(const TransportConnection&) = delete;
  TransportConnection& operator=(const TransportConnection&) = delete;
  TransportConnection(TransportConnection&&) = delete;
  TransportConnection& operator=(TransportConnection&&) = delete;

  [[nodiscard]] bool connect_to(config::ServerProfile profile);
  [[nodiscard]] bool disconnect();
  [[nodiscard]] bool reconnect();
  void send(const QJsonObject& message);

  [[nodiscard]] TransportState state() const;
  [[nodiscard]] bool is_connected() const;
  [[nodiscard]] bool has_profile() const;
  [[nodiscard]] const std::optional<config::ServerProfile>& profile() const;
  [[nodiscard]] Transport* transport() const;

signals:
  void messageReceived(const QJsonObject& message);
  void stderrReceived(const QString& text);
  void errorOccurred(const transport::TransportError& error);
  void stateChanged(transport::TransportState state);

private:
  [[nodiscard]] bool has_transport() const;
  [[nodiscard]] bool should_stop_before_connect() const;
  void wire_transport_signals();
  void emit_missing_transport_error();

  std::unique_ptr<Transport> transport_;
  std::optional<config::ServerProfile> profile_;
};

} // namespace transport
