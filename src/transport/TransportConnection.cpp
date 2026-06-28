#include "TransportConnection.hpp"

#include <utility>

namespace transport {

TransportConnection::TransportConnection(std::unique_ptr<Transport> transport, QObject* parent)
  : QObject(parent)
  , transport_(std::move(transport))
{
  wire_transport_signals();
}

TransportConnection::~TransportConnection() = default;

bool TransportConnection::connect_to(config::ServerProfile profile)
{
  if (!has_transport()) {
    emit_missing_transport_error();
    return false;
  }

  if (should_stop_before_connect()) {
    transport_->stop();
  }

  profile_ = std::move(profile);
  transport_->start(*profile_);
  return state() == TransportState::Starting || state() == TransportState::Running;
}

bool TransportConnection::disconnect()
{
  if (!has_transport()) {
    emit_missing_transport_error();
    return false;
  }

  if (!should_stop_before_connect()) {
    return false;
  }

  transport_->stop();
  return state() == TransportState::Stopped || state() == TransportState::Idle;
}

bool TransportConnection::reconnect()
{
  if (!profile_.has_value()) {
    return false;
  }

  auto next_profile = *profile_;
  if (has_transport() && should_stop_before_connect()) {
    transport_->stop();
  }

  return connect_to(std::move(next_profile));
}

void TransportConnection::send(const QJsonObject& message)
{
  if (!has_transport()) {
    emit_missing_transport_error();
    return;
  }

  transport_->send(message);
}

TransportState TransportConnection::state() const
{
  return has_transport() ? transport_->state() : TransportState::Failed;
}

bool TransportConnection::is_connected() const
{
  return state() == TransportState::Running;
}

bool TransportConnection::has_profile() const
{
  return profile_.has_value();
}

const std::optional<config::ServerProfile>& TransportConnection::profile() const
{
  return profile_;
}

Transport* TransportConnection::transport() const
{
  return transport_.get();
}

bool TransportConnection::has_transport() const
{
  return transport_ != nullptr;
}

bool TransportConnection::should_stop_before_connect() const
{
  if (!has_transport()) {
    return false;
  }

  const auto current_state = transport_->state();
  return current_state == TransportState::Starting || current_state == TransportState::Running
      || current_state == TransportState::Stopping;
}

void TransportConnection::wire_transport_signals()
{
  if (!has_transport()) {
    return;
  }

  connect(transport_.get(), &Transport::messageReceived, this, &TransportConnection::messageReceived);
  connect(transport_.get(), &Transport::stderrReceived, this, &TransportConnection::stderrReceived);
  connect(transport_.get(), &Transport::errorOccurred, this, &TransportConnection::errorOccurred);
  connect(transport_.get(), &Transport::stateChanged, this, &TransportConnection::stateChanged);
}

void TransportConnection::emit_missing_transport_error()
{
  TransportError error;
  error.category = TransportErrorCategory::Unknown;
  error.message = QStringLiteral("transport connection has no transport instance");
  emit errorOccurred(error);
  emit stateChanged(TransportState::Failed);
}

} // namespace transport
