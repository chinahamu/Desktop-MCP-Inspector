#include "StdioJsonLineParser.hpp"

#include <QJsonDocument>
#include <QJsonParseError>

#include <optional>
#include <utility>

namespace transport {
namespace {

void trim_trailing_carriage_return(QByteArray& line)
{
  if (line.endsWith('\r')) {
    line.chop(1);
  }
}

} // namespace

StdioJsonLineParserResult StdioJsonLineParser::append(QByteArray data)
{
  StdioJsonLineParserResult result;
  if (data.isEmpty()) {
    return result;
  }

  buffer_.append(std::move(data));

  qsizetype newline_index = buffer_.indexOf('\n');
  while (newline_index >= 0) {
    auto line = buffer_.left(newline_index);
    buffer_.remove(0, newline_index + 1);
    trim_trailing_carriage_return(line);

    if (!line.trimmed().isEmpty()) {
      StdioJsonLineParseError error;
      auto message = parse_line(std::move(line), error);
      if (message.has_value()) {
        result.messages.push_back(std::move(*message));
      } else {
        result.errors.push_back(std::move(error));
      }
    }

    newline_index = buffer_.indexOf('\n');
  }

  return result;
}

void StdioJsonLineParser::clear()
{
  buffer_.clear();
}

bool StdioJsonLineParser::has_pending_line() const
{
  return !buffer_.isEmpty();
}

QByteArray StdioJsonLineParser::pending_line() const
{
  return buffer_;
}

std::optional<QJsonObject> StdioJsonLineParser::parse_line(
    QByteArray line,
    StdioJsonLineParseError& error)
{
  QJsonParseError parse_error;
  const auto document = QJsonDocument::fromJson(line, &parse_error);
  if (parse_error.error != QJsonParseError::NoError) {
    error.raw_line = std::move(line);
    error.message = parse_error.errorString();
    error.offset = parse_error.offset;
    return std::nullopt;
  }

  if (!document.isObject()) {
    error.raw_line = std::move(line);
    error.message = QStringLiteral("stdout line must be a JSON object");
    error.offset = -1;
    return std::nullopt;
  }

  return document.object();
}

} // namespace transport
