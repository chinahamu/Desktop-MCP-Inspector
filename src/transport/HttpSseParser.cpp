#include "HttpSseParser.hpp"

#include <utility>

namespace transport {

std::vector<HttpSseEvent> HttpSseParser::append(const QByteArray& chunk)
{
  buffer_.append(chunk);
  std::vector<HttpSseEvent> events;

  while (true) {
    const auto newline = buffer_.indexOf('\n');
    if (newline < 0) {
      break;
    }

    auto raw_line = buffer_.left(newline);
    buffer_.remove(0, newline + 1);
    if (raw_line.endsWith('\r')) {
      raw_line.chop(1);
    }

    auto line_events = consume_line(QString::fromUtf8(raw_line));
    events.insert(events.end(), line_events.begin(), line_events.end());
  }

  return events;
}

void HttpSseParser::clear()
{
  buffer_.clear();
  reset_event();
}

std::vector<HttpSseEvent> HttpSseParser::consume_line(QString line)
{
  std::vector<HttpSseEvent> events;
  if (line.isEmpty()) {
    if (!current_event_.isEmpty() || !current_data_.isEmpty()) {
      events.push_back(HttpSseEvent{current_event_, current_data_});
    }
    reset_event();
    return events;
  }

  if (line.startsWith(QChar(':'))) {
    return events;
  }

  const auto separator = line.indexOf(QChar(':'));
  const auto field = separator < 0 ? line : line.left(separator);
  auto value = separator < 0 ? QString() : line.mid(separator + 1);
  if (value.startsWith(QChar(' '))) {
    value.remove(0, 1);
  }

  if (field == QStringLiteral("event")) {
    current_event_ = std::move(value);
  } else if (field == QStringLiteral("data")) {
    if (!current_data_.isEmpty()) {
      current_data_.append(QChar('\n'));
    }
    current_data_.append(value);
  }

  return events;
}

void HttpSseParser::reset_event()
{
  current_event_.clear();
  current_data_.clear();
}

} // namespace transport
