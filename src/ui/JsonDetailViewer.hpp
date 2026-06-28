#pragma once

#include <QJsonObject>
#include <QString>
#include <QWidget>

class QTextEdit;

namespace ui {

class JsonDetailViewer final : public QWidget
{
  Q_OBJECT

public:
  explicit JsonDetailViewer(QWidget* parent = nullptr);

  void set_json(const QJsonObject& object);
  void set_placeholder(QString text);
  [[nodiscard]] QString text() const;

private:
  QTextEdit* editor_ = nullptr;
};

} // namespace ui
