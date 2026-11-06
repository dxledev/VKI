#include "cli.h"
#include "core/storage.h"
#include "accent_sync.h"
#include "core/theme.h"
#include "device/definition.h"
#include "ui/window.h"
#include <QApplication>
#include <QDir>
#include <QLockFile>
#include <QTextStream>
int main(int argc, char **argv) {
  QCoreApplication::setApplicationName("vki");
  QCoreApplication::setApplicationVersion("1.0.0");
  QStringList args;
  for (int i = 1; i < argc; ++i)
    args.append(QString::fromLocal8Bit(argv[i]));
  try {
    bool daemon = false;
    for (qsizetype i = 0; i < args.size(); ++i) {
      if (args[i] == "--config-dir" || args[i] == "--definition") { ++i; continue; }
      if (args[i] == "accent-daemon") daemon = true;
    }
    if (daemon) {
      QCoreApplication app(argc, argv);
      QString definitionPath = ":/vki/galaxy100-lite.json";
      for (qsizetype i = 0; i + 1 < args.size(); ++i)
        if (args[i] == "--definition") definitionPath = args[i + 1];
      return vki::runAccentDaemon(vki::configDirectory(args), definitionPath);
    }
    bool command = false;
    for (const auto &arg : args)
      if (arg == "theme" || arg == "--help" || arg == "-h" ||
          arg == "--version")
        command = true;
    if (command) {
      QCoreApplication app(argc, argv);
      return vki::runCli(args);
    }
    const auto result = vki::runCli(args);
    if (result != -1)
      return result;
    const auto directory = vki::configDirectory(args);
    if (!qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY") &&
        qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
      qputenv("QT_QPA_PLATFORM", "wayland");
    QApplication app(argc, argv);
    app.setStyle("Fusion");
    app.setDesktopFileName("io.github.vki");
    QDir().mkpath(directory);
    QLockFile session(directory + "/session.lock");
    if (!session.tryLock(0)) {
      QTextStream(stderr) << "VKI is already running with this configuration.\n";
      return 1;
    }
    auto theme = vki::ensureTheme(directory + "/theme.json");
    QString definitionPath = ":/vki/galaxy100-lite.json";
    for (qsizetype i = 0; i + 1 < args.size(); ++i)
      if (args[i] == "--definition") definitionPath = args[i + 1];
    vki::Window window(directory, theme, vki::loadDefinition(definitionPath));
    window.show();
    return app.exec();
  } catch (const std::exception &error) {
    QTextStream(stderr) << "vki: " << error.what() << '\n';
    return 1;
  }
}
