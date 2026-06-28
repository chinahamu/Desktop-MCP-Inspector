#include "StdioProcessError.hpp"

#include <QProcess>

namespace transport {

TransportErrorCategory stdio_process_error_category(QProcess::ProcessError error)
{
  switch (error) {
  case QProcess::FailedToStart:
    return TransportErrorCategory::StartFailed;
  case QProcess::WriteError:
    return TransportErrorCategory::WriteFailed;
  case QProcess::ReadError:
    return TransportErrorCategory::ReadFailed;
  case QProcess::Timedout:
    return TransportErrorCategory::Timeout;
  case QProcess::Crashed:
    return TransportErrorCategory::ProcessExited;
  case QProcess::UnknownError:
    return TransportErrorCategory::Unknown;
  }

  return TransportErrorCategory::Unknown;
}

QString stdio_process_error_message(QProcess::ProcessError error)
{
  switch (error) {
  case QProcess::FailedToStart:
    return QStringLiteral("stdio process failed to start");
  case QProcess::Crashed:
    return QStringLiteral("stdio process crashed");
  case QProcess::Timedout:
    return QStringLiteral("stdio process operation timed out");
  case QProcess::WriteError:
    return QStringLiteral("failed to write to stdio process stdin");
  case QProcess::ReadError:
    return QStringLiteral("failed to read from stdio process stdout/stderr");
  case QProcess::UnknownError:
    return QStringLiteral("unknown stdio process error");
  }

  return QStringLiteral("unknown stdio process error");
}

QString stdio_exit_message(const StdioProcessExit& process_exit)
{
  if (process_exit.requested_stop) {
    return QStringLiteral("stdio process stopped");
  }

  if (process_exit.crashed) {
    return QStringLiteral("stdio process crashed");
  }

  return QStringLiteral("stdio process exited unexpectedly with code %1").arg(process_exit.exit_code);
}

QJsonObject stdio_exit_details(const StdioProcessExit& process_exit)
{
  return QJsonObject{
      {QStringLiteral("exit_code"), process_exit.exit_code},
      {QStringLiteral("crashed"), process_exit.crashed},
      {QStringLiteral("requested_stop"), process_exit.requested_stop},
  };
}

TransportError stdio_exit_error(const StdioProcessExit& process_exit)
{
  TransportError error;
  error.category = TransportErrorCategory::ProcessExited;
  error.message = stdio_exit_message(process_exit);
  error.exit_code = process_exit.exit_code;
  error.details = stdio_exit_details(process_exit);
  return error;
}

bool is_unexpected_process_exit(const StdioProcessExit& process_exit)
{
  return !process_exit.requested_stop;
}

} // namespace transport
