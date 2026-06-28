#include "Transport.hpp"

namespace transport {

Transport::Transport(QObject* parent)
  : QObject(parent)
{
}

Transport::~Transport() = default;

bool Transport::is_running() const
{
  return state() == TransportState::Running;
}

QString transport_state_name(TransportState state)
{
  switch (state) {
  case TransportState::Idle:
    return QStringLiteral("idle");
  case TransportState::Starting:
    return QStringLiteral("starting");
  case TransportState::Running:
    return QStringLiteral("running");
  case TransportState::Stopping:
    return QStringLiteral("stopping");
  case TransportState::Stopped:
    return QStringLiteral("stopped");
  case TransportState::Failed:
    return QStringLiteral("failed");
  }

  return QStringLiteral("unknown");
}

QString transport_error_category_name(TransportErrorCategory category)
{
  switch (category) {
  case TransportErrorCategory::StartFailed:
    return QStringLiteral("start_failed");
  case TransportErrorCategory::WriteFailed:
    return QStringLiteral("write_failed");
  case TransportErrorCategory::ReadFailed:
    return QStringLiteral("read_failed");
  case TransportErrorCategory::ProtocolViolation:
    return QStringLiteral("protocol_violation");
  case TransportErrorCategory::ProcessExited:
    return QStringLiteral("process_exited");
  case TransportErrorCategory::Timeout:
    return QStringLiteral("timeout");
  case TransportErrorCategory::HttpError:
    return QStringLiteral("http_error");
  case TransportErrorCategory::JsonRpcError:
    return QStringLiteral("json_rpc_error");
  case TransportErrorCategory::Unknown:
    return QStringLiteral("unknown");
  }

  return QStringLiteral("unknown");
}

} // namespace transport
