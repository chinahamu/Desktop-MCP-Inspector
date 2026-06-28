#include "JsonDetailViewer.hpp"

#include <QJsonDocument>
#include <QTextEdit>
#include <QVBoxLayout>

#include <utility>

namespace ui {

JsonDetailViewer::JsonDetailViewer(QWidget* parent)
  : QWidget(parent)
{
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  editor_ = new QTextEdit(this);
  editor_->setReadOnly(true);
  editor_->setLineWrapMode(QTextEdit::NoWrap);
  editor_->setPlaceholderText(tr("Select a timeline row to inspect raw JSON."));
  layout->addWidget(editor_);
}

void JsonDetailViewer::set_json(const QJsonObject& object)
{
  editor_->setPlainText(QString::fromUtf8(QJsonDocument{object}.toJson(QJsonDocument::Indented)));
}

void JsonDetailViewer::set_placeholder(QString text)
{
  editor_->clear();
  editor_->setPlaceholderText(std::move(text));
}

QString JsonDetailViewer::text() const
{
  return editor_->toPlainText();
}

} // namespace ui
