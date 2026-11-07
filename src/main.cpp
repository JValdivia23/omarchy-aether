#include "backend.h"
#include "cli.h"
#include "theme.h"
#include "translator.h"
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

int main(int argc, char *argv[]) {
    const bool graphical = argc == 1 || QByteArray(argv[1]) == "open";
    if (!graphical) qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    app.setApplicationName("omarchy-aether");
    app.setApplicationVersion("0.1.0");
    app.setDesktopFileName("omarchy-aether");
    app.setWindowIcon(QIcon::fromTheme("omarchy-aether"));
    if (!graphical) return runCli();
    QQuickStyle::setStyle("Material");
    Translator translator;
    app.installTranslator(&translator);
    Theme theme;
    Backend backend;
    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    QObject::connect(&translator, &Translator::changed, &backend, &Backend::refreshLanguage);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);
    if (argc > 2 && QByteArray(argv[1]) == "open") {
        backend.setImage(QUrl::fromLocalFile(QString::fromLocal8Bit(argv[2])));
        backend.primaryAction();
    }
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty()) return 1;
    return app.exec();
}
