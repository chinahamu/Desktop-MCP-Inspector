#pragma once

#include <QByteArray>
#include <QString>

#include <vector>

namespace transport {

struct HttpSseEvent
{
  QString event;
  QString data;

  friend bool operator==(const HttpSseEvent& lhs, const HttpSseEvent& rhs) = default;
};

class HttpSseParser final
{
public:
  [[nodiscard]] std::vector<HttpSseEvent> append(const QByteArray& chunk);
  void clear();

private:
  [[nodiscard]] std::vector<HttpSseEvent> consume_line(QString line);
  void reset_event();

  QByteArray buffer_;
  QString current_event_;
  QString current_data_;
};

} // namespace transport
