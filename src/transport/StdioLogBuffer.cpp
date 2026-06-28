#include "StdioLogBuffer.hpp"

#include <utility>

namespace transport {
namespace {

void trim_trailing_carriage_return(QByteArray& line)
{
  if (line.endsWith('\r')) {
    line.chop(1);
  }
}

[[nodiscard]] QString to_log_line(QByteArray line)
{
  trim_trailing_carriage_return(line);
  return QString::fromUtf8(line);
}

} // namespace

std::vector<QString> StdioLogBuffer::append(QByteArray data)
{
  std::vector<QString> lines;
  if (data.isEmpty()) {
    return lines;
  }

  buffer_.append(std::move(data));

  qsizetype newline_index = buffer_.indexOf('\n');
  while (newline_index >= 0) {
    auto line = buffer_.left(newline_index);
    buffer_.remove(0, newline_index + 1);
    lines.push_back(to_log_line(std::move(line)));
    newline_index = buffer_.indexOf('\n');
  }

  return lines;
}

std::vector<QString> StdioLogBuffer::flush()
{
  std::vector<QString> lines;
  if (buffer_.isEmpty()) {
    return lines;
  }

  lines.push_back(to_log_line(std::move(buffer_)));
  buffer_.clear();
  return lines;
}

void StdioLogBuffer::clear()
{
  buffer_.clear();
}

bool StdioLogBuffer::has_pending_line() const
{
  return !buffer_.isEmpty();
}

QByteArray StdioLogBuffer::pending_line() const
{
  return buffer_;
}

} // namespace transport
