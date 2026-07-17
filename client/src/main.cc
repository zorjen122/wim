#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QTimer>

namespace {

bool HasArgument(const QStringList &arguments, const QString &expected) {
  return arguments.contains(expected);
}

QString ArgumentValue(const QStringList &arguments, const QString &name) {
  const QString prefix = QStringLiteral("--") + name + QStringLiteral("=");
  for (const auto &argument : arguments) {
    if (argument.startsWith(prefix)) {
      return argument.mid(prefix.size());
    }
  }
  return {};
}

}  // namespace

int main(int argc, char *argv[]) {
  QQuickStyle::setStyle(QStringLiteral("Basic"));

  QGuiApplication application(argc, argv);
  QCoreApplication::setApplicationName(QStringLiteral("WIM"));
  QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));
  QCoreApplication::setOrganizationName(QStringLiteral("WIM"));
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
  QGuiApplication::setDesktopFileName(QStringLiteral("wim-client"));
#endif
  QGuiApplication::setWindowIcon(
      QIcon(QStringLiteral(":/icons/wim-client.svg")));

  QQmlApplicationEngine engine;
  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreationFailed, &application,
      [] { QCoreApplication::exit(EXIT_FAILURE); }, Qt::QueuedConnection);
  engine.loadFromModule(QStringLiteral("Wim.Client"), QStringLiteral("Main"));

  const QString screenshotPath =
      ArgumentValue(application.arguments(), QStringLiteral("screenshot"));
  if (!screenshotPath.isEmpty()) {
    QTimer::singleShot(
        500, &application, [&application, &engine, screenshotPath] {
          auto *window =
              qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
          const bool saved =
              window != nullptr && window->grabWindow().save(screenshotPath);
          application.exit(saved ? EXIT_SUCCESS : EXIT_FAILURE);
        });
  } else if (HasArgument(application.arguments(),
                         QStringLiteral("--smoke-test"))) {
    QTimer::singleShot(300, &application, &QCoreApplication::quit);
  }

  return application.exec();
}
