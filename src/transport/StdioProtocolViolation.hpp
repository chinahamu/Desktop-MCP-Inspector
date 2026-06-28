#pragma once

#include <QJsonObject>
#include <QString>

#include <optional>

namespace transport {

struct StdioProtocolViolation
{
  QString code;
  QString message;
  QJsonObject details;

  friend bool operator==(const StdioProtocolViolation& lhs, const StdioProtocolViolation& rhs) = default;
};

[[nodiscard]] std::optional<StdioProtocolViolation> detect_stdio_protocol_violation(
    const QJsonObject& message);

} // namespace transport
