// SPDX-License-Identifier: GPL-3.0-or-later
#include <wp/wp.h>

#include "backend/FixtureGraphSource.hpp"
#include "backend/WirePlumberGraphSource.hpp"
#include "presentation/GraphController.hpp"
#include "presentation/LinkLayer.hpp"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTimer>
#include <QQuickWindow>

using namespace wirerunner;

int main(int argc, char **argv) {
  QGuiApplication app(argc, argv);
  QGuiApplication::setOrganizationDomain(QStringLiteral("io.github.tucktuckg00se"));
  QGuiApplication::setApplicationName(QStringLiteral("WireRunner"));
  QGuiApplication::setApplicationVersion(QStringLiteral("0.3.1"));
  QGuiApplication::setDesktopFileName(QStringLiteral("io.github.tucktuckg00se.WireRunner"));

  QCommandLineParser parser;
  parser.setApplicationDescription(QStringLiteral("PipeWire and WirePlumber graph control surface"));
  parser.addHelpOption();
  parser.addVersionOption();
  QCommandLineOption demo(QStringLiteral("demo"), QStringLiteral("Use the bundled demonstration graph"));
  QCommandLineOption fixture(QStringLiteral("fixture"), QStringLiteral("Load a graph fixture"), QStringLiteral("path"));
  QCommandLineOption quitAfter(QStringLiteral("quit-after"),
    QStringLiteral("Exit after a number of milliseconds (for automated checks)"), QStringLiteral("milliseconds"));
  QCommandLineOption screenshot(QStringLiteral("screenshot"),
    QStringLiteral("Save an offscreen screenshot and exit"), QStringLiteral("path"));
  parser.addOption(demo);
  parser.addOption(fixture);
  parser.addOption(quitAfter);
  parser.addOption(screenshot);
  parser.process(app);

  wp_init(WP_INIT_ALL);
  std::unique_ptr<GraphSource> source;
  if (parser.isSet(fixture)) source = std::make_unique<FixtureGraphSource>(parser.value(fixture));
  else if (parser.isSet(demo)) source = std::make_unique<FixtureGraphSource>(QStringLiteral(":/fixtures/demo-graph.json"));
  else source = std::make_unique<WirePlumberGraphSource>();

  qmlRegisterType<LinkLayer>("WireRunner", 1, 0, "LinkLayer");
  int result = 1;
  {
    GraphController controller(std::move(source));
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("graph"), &controller);
    engine.loadFromModule(QStringLiteral("WireRunner"), QStringLiteral("Main"));
    if (!engine.rootObjects().isEmpty()) {
      controller.start();
      if (parser.isSet(quitAfter)) {
        bool valid = false;
        const int milliseconds = parser.value(quitAfter).toInt(&valid);
        if (valid && milliseconds >= 0) QTimer::singleShot(milliseconds, &app, &QCoreApplication::quit);
      }
      if (parser.isSet(screenshot)) {
        QTimer::singleShot(500, &app, [&app, &engine, path = parser.value(screenshot)] {
          if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst()))
            window->grabWindow().save(path);
          app.quit();
        });
      }
      result = app.exec();
    }
  }
  return result;
}
