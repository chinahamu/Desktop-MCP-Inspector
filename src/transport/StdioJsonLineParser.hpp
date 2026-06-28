#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>

#include <optional>
#include <vector>

namespace transport {

struct StdioJsonLineParseError
{
  QByteArray raw_line;
  QString message;
  qsizetype offset = -1;

  friend bool operator==(const StdioJsonLineParseError& lhs, const StdioJsonLineParseError& rhs) = default;
};

struct StdioJsonLineParserResult
{
  std::vector<QJsonObject> messages;
  std::vector<StdioJsonLineParseError> errors;
};

class StdioJsonLineParser
{
public:
  [[nodiscard]] StdioJsonLineParserResult append(QByteArray data);
  void clear();

  [[nodiscard]] bool has_pending_line() const;
  [[nodiscard]] QByteArray pending_line() const;

private:
  [[nodiscard]] static std::optional<QJsonObject> parse_line(
      QByteArray line,
      StdioJsonLineParseError& error);

  QByteArray buffer_;
};

} // namespace transport
