#include "AppSettings.hpp"
#include "MainWindow.hpp"

#include <QApplication>
#include <QIcon>
#include <QSettings>

int main(int argc, char* argv[])
{
  QApplication app(argc, argv);
  app::apply_application_metadata(app);
  app.setWindowIcon(QIcon(QStringLiteral(":/icon.png")));

  auto settings = app::create_user_settings();
  settings.setValue(QStringLiteral("app/lastStartedVersion"), app.applicationVersion());

  MainWindow window;
  window.show();

  return QApplication::exec();
}
