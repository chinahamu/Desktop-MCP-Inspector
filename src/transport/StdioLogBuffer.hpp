#pragma once

#include <QByteArray>
#include <QString>

#include <vector>

namespace transport {

class StdioLogBuffer
{
public:
  [[nodiscard]] std::vector<QString> append(QByteArray data);
  [[nodiscard]] std::vector<QString> flush();
  void clear();

  [[nodiscard]] bool has_pending_line() const;
  [[nodiscard]] QByteArray pending_line() const;

private:
  QByteArray buffer_;
};

} // namespace transport
