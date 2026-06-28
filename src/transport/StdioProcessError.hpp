#pragma once

#include "Transport.hpp"

#include <QJsonObject>
#include <QProcess>
#include <QString>

namespace transport {

struct StdioProcessExit
{
  int exit_code = 0;
  bool crashed = false;
  bool requested_stop = false;
};

[[nodiscard]] TransportErrorCategory stdio_process_error_category(QProcess::ProcessError error);
[[nodiscard]] QString stdio_process_error_message(QProcess::ProcessError error);
[[nodiscard]] QString stdio_exit_message(const StdioProcessExit& process_exit);
[[nodiscard]] QJsonObject stdio_exit_details(const StdioProcessExit& process_exit);
[[nodiscard]] TransportError stdio_exit_error(const StdioProcessExit& process_exit);
[[nodiscard]] bool is_unexpected_process_exit(const StdioProcessExit& process_exit);

} // namespace transport
