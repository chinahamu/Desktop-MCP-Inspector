#include "StdioTransport.hpp"

#include "ServerProfile.hpp"
#include "StdioLaunchSpec.hpp"
#include "StdioProcessError.hpp"

#include <QJsonDocument>
#include <QProcess>
#include <QProcessEnvironment>

#include <utility>

namespace transport {
namespace {

constexpr qint64 kDefaultStartTimeoutMs = 5000;
constexpr int kTerminateTimeoutMs = 2000;
constexpr int kKillTimeoutMs = 1000;

} // namespace

StdioTransport::StdioTransport(QObject* parent)
  : Transport(parent)
{
}

StdioTransport::~StdioTransport()
{
  stop();
}

void StdioTransport::start(const config::ServerProfile& profile)
{
  if (!config::is_stdio_profile(profile)) {
    emit_error(
        TransportErrorCategory::StartFailed,
        QStringLiteral("StdioTransport can only start stdio server profiles"));
    set_state(TransportState::Failed);
    return;
  }

  if (profile.command.trimmed().isEmpty()) {
    emit_error(TransportErrorCategory::StartFailed, QStringLiteral("stdio server command is empty"));
    set_state(TransportState::Failed);
    return;
  }

  if (process_ != nullptr && process_->state() != QProcess::NotRunning) {
    stop();
  }

  stdout_parser_.clear();
  stderr_buffer_.clear();
  stop_requested_ = false;
  start_failure_reported_ = false;
  process_ = std::make_unique<QProcess>();
  configure_process(profile);
  connect_process_signals();

  set_state(TransportState::Starting);
  process_->start();

  if (!process_->waitForStarted(start_timeout_ms(profile))) {
    if (!start_failure_reported_) {
      const auto error_string = process_->errorString();
      emit_error(
          TransportErrorCategory::StartFailed,
          error_string.isEmpty() ? QStringLiteral("stdio process failed to start") : error_string);
      start_failure_reported_ = true;
    }

    process_.reset();
    set_state(TransportState::Failed);
    return;
  }

  set_state(TransportState::Running);
}

void StdioTransport::stop()
{
  stop_requested_ = true;

  if (process_ == nullptr) {
    flush_stderr_logs();
    if (state_ != TransportState::Idle && state_ != TransportState::Failed) {
      set_state(TransportState::Stopped);
    }
    return;
  }

  if (process_->state() == QProcess::NotRunning) {
    flush_stderr_logs();
    process_.reset();
    if (state_ != TransportState::Failed) {
      set_state(TransportState::Stopped);
    }
    return;
  }

  set_state(TransportState::Stopping);
  process_->terminate();

  if (!process_->waitForFinished(kTerminateTimeoutMs)) {
    process_->kill();
    process_->waitForFinished(kKillTimeoutMs);
  }

  handle_stderr_ready();
  flush_stderr_logs();
  process_.reset();
  set_state(TransportState::Stopped);
}

void StdioTransport::send(const QJsonObject& message)
{
  if (process_ == nullptr || process_->state() != QProcess::Running) {
    emit_error(TransportErrorCategory::WriteFailed, QStringLiteral("stdio process is not running"));
    return;
  }

  auto payload = QJsonDocument{message}.toJson(QJsonDocument::Compact);
  payload.append('\n');

  if (process_->write(payload) == -1) {
    emit_error(TransportErrorCategory::WriteFailed, process_->errorString());
  }
}

TransportState StdioTransport::state() const
{
  return state_;
}

void StdioTransport::configure_process(const config::ServerProfile& profile)
{
  const auto spec = make_stdio_launch_spec(profile);

  process_->setProgram(spec.program);
  process_->setArguments(spec.arguments);
  process_->setProcessChannelMode(QProcess::SeparateChannels);

  if (!spec.working_directory.trimmed().isEmpty()) {
    process_->setWorkingDirectory(spec.working_directory);
  }

  auto environment = QProcessEnvironment::systemEnvironment();
  for (auto iterator = spec.environment.cbegin(); iterator != spec.environment.cend(); ++iterator) {
    environment.insert(iterator.key(), iterator.value());
  }
  process_->setProcessEnvironment(environment);
}

void StdioTransport::connect_process_signals()
{
  connect(process_.get(), &QProcess::readyReadStandardOutput, this, &StdioTransport::handle_stdout_ready);
  connect(process_.get(), &QProcess::readyReadStandardError, this, &StdioTransport::handle_stderr_ready);
  connect(process_.get(), &QProcess::errorOccurred, this, &StdioTransport::handle_process_error);
  connect(
      process_.get(),
      qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
      this,
      [this](int exit_code, QProcess::ExitStatus exit_status) {
        handle_stderr_ready();
        flush_stderr_logs();
        handle_process_finished(exit_code, exit_status == QProcess::CrashExit);
      });
}

void StdioTransport::set_state(TransportState next_state)
{
  if (state_ == next_state) {
    return;
  }

  state_ = next_state;
  emit stateChanged(state_);
}

void StdioTransport::emit_error(TransportErrorCategory category, QString message)
{
  TransportError error;
  error.category = category;
  error.message = std::move(message);
  emit_transport_error(error);
}

void StdioTransport::emit_transport_error(const TransportError& error)
{
  emit errorOccurred(error);
}

void StdioTransport::emit_parse_error(const StdioJsonLineParseError& error)
{
  TransportError transport_error;
  transport_error.category = TransportErrorCategory::ProtocolViolation;
  transport_error.message = QStringLiteral("stdout line was not a JSON object: %1").arg(error.message);
  transport_error.details = QJsonObject{
      {QStringLiteral("raw"), QString::fromUtf8(error.raw_line)},
      {QStringLiteral("offset"), static_cast<qint64>(error.offset)},
  };
  emit_transport_error(transport_error);
}

void StdioTransport::emit_protocol_violation(const StdioProtocolViolation& violation)
{
  TransportError transport_error;
  transport_error.category = TransportErrorCategory::ProtocolViolation;
  transport_error.message = QStringLiteral("stdout JSON-RPC protocol violation: %1").arg(violation.message);
  transport_error.details = violation.details;
  emit_transport_error(transport_error);
}

void StdioTransport::emit_stderr_lines(const std::vector<QString>& lines)
{
  for (const auto& line : lines) {
    emit stderrReceived(line);
  }
}

void StdioTransport::flush_stderr_logs()
{
  emit_stderr_lines(stderr_buffer_.flush());
}

void StdioTransport::handle_stdout_ready()
{
  if (process_ == nullptr) {
    return;
  }

  auto result = stdout_parser_.append(process_->readAllStandardOutput());
  for (const auto& message : result.messages) {
    if (const auto violation = detect_stdio_protocol_violation(message); violation.has_value()) {
      emit_protocol_violation(*violation);
      continue;
    }

    emit messageReceived(message);
  }

  for (const auto& error : result.errors) {
    emit_parse_error(error);
  }
}

void StdioTransport::handle_stderr_ready()
{
  if (process_ == nullptr) {
    return;
  }

  emit_stderr_lines(stderr_buffer_.append(process_->readAllStandardError()));
}

void StdioTransport::handle_process_error()
{
  if (process_ == nullptr) {
    return;
  }

  const auto error = process_->error();
  if (error == QProcess::Crashed) {
    set_state(TransportState::Failed);
    return;
  }

  if (error == QProcess::FailedToStart) {
    start_failure_reported_ = true;
  }

  TransportError transport_error;
  transport_error.category = stdio_process_error_category(error);
  transport_error.message = process_->errorString().isEmpty()
      ? stdio_process_error_message(error)
      : process_->errorString();
  transport_error.details = QJsonObject{
      {QStringLiteral("process_error"), static_cast<int>(error)},
      {QStringLiteral("requested_stop"), stop_requested_},
  };
  emit_transport_error(transport_error);

  if (error == QProcess::FailedToStart) {
    set_state(TransportState::Failed);
  }
}

void StdioTransport::handle_process_finished(int exit_code, bool crashed)
{
  const StdioProcessExit process_exit{exit_code, crashed, stop_requested_};
  if (is_unexpected_process_exit(process_exit)) {
    emit_transport_error(stdio_exit_error(process_exit));
    set_state(TransportState::Failed);
    return;
  }

  set_state(TransportState::Stopped);
}

qint64 StdioTransport::start_timeout_ms(const config::ServerProfile& profile) const
{
  if (!profile.timeout_ms.has_value()) {
    return kDefaultStartTimeoutMs;
  }

  return *profile.timeout_ms > 0 ? *profile.timeout_ms : kDefaultStartTimeoutMs;
}

} // namespace transport
