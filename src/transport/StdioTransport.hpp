#pragma once

#include "StdioJsonLineParser.hpp"
#include "StdioLogBuffer.hpp"
#include "StdioProtocolViolation.hpp"
#include "Transport.hpp"

#include <QString>
#include <QtTypes>

#include <memory>
#include <vector>

class QProcess;

namespace transport {

class StdioTransport final : public Transport
{
  Q_OBJECT

public:
  explicit StdioTransport(QObject* parent = nullptr);
  ~StdioTransport() override;

  void start(const config::ServerProfile& profile) override;
  void stop() override;
  void send(const QJsonObject& message) override;

  [[nodiscard]] TransportState state() const override;

private:
  void configure_process(const config::ServerProfile& profile);
  void connect_process_signals();
  void set_state(TransportState next_state);
  void emit_error(TransportErrorCategory category, QString message);
  void emit_transport_error(const TransportError& error);
  void emit_parse_error(const StdioJsonLineParseError& error);
  void emit_protocol_violation(const StdioProtocolViolation& violation);
  void emit_stderr_lines(const std::vector<QString>& lines);
  void flush_stderr_logs();
  void handle_stdout_ready();
  void handle_stderr_ready();
  void handle_process_error();
  void handle_process_finished(int exit_code, bool crashed);

  [[nodiscard]] qint64 start_timeout_ms(const config::ServerProfile& profile) const;

  std::unique_ptr<QProcess> process_;
  StdioJsonLineParser stdout_parser_;
  StdioLogBuffer stderr_buffer_;
  TransportState state_ = TransportState::Idle;
  bool stop_requested_ = false;
  bool start_failure_reported_ = false;
};

} // namespace transport
