// gander — Quick Look for Linux: press Space on a file, see it.
//
// `gander daemon` is the previewer itself, a resident service that owns
// Nautilus's previewer name on D-Bus. `gander FILE` is a tiny client that asks
// the daemon to show FILE and returns at once, so binding it to a key or
// calling it from a terminal file manager costs a D-Bus round trip, not an
// application start.

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTextStream>
#include <QTimer>
#include <QPainter>
#include <QUrl>

#include <cstdio>

#include "imagestore.h"
#include "inspector.h"
#include "overlay.h"
#include "pdf.h"
#include "previewer.h"
#include "theme.h"
#include "translator.h"

static const char *usage =
    "Usage: gander FILE      preview FILE (again to close)\n"
    "       gander close     close the preview\n"
    "       gander daemon    run the previewer (started for you by systemd or D-Bus)\n"
    "       gander --version\n";

static int runClient(int argc, char *argv[], const QStringList &arguments) {
    QCoreApplication app(argc, argv);
    QDBusInterface previewer(Previewer::serviceName(), Previewer::objectPath(), "org.gnome.NautilusPreviewer2",
                             QDBusConnection::sessionBus());
    // The first call may start the daemon through D-Bus activation.
    previewer.setTimeout(10000);
    QDBusMessage reply;
    if (arguments.first() == "close") {
        reply = previewer.call("Close");
    } else {
        const QFileInfo info(arguments.first());
        if (!info.exists()) {
            std::fprintf(stderr, "gander: %s: no such file or folder\n", qPrintable(arguments.first()));
            return 1;
        }
        reply = previewer.call("ShowFile", Inspector::fileUri(info.absoluteFilePath()), QString("gander-cli"), true,
                               QString());
    }
    if (reply.type() == QDBusMessage::ErrorMessage) {
        std::fprintf(stderr, "gander: %s\n", qPrintable(reply.errorMessage()));
        return 1;
    }
    return 0;
}

static int runDaemon(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    app.setApplicationName("gander");
    app.setApplicationVersion(GANDER_VERSION);
    app.setDesktopFileName("gander");
    app.setWindowIcon(QIcon::fromTheme("gander"));
    // The window hides between previews; that is not a reason to exit.
    app.setQuitOnLastWindowClosed(false);

    QQuickStyle::setStyle("Material");

    Translator translator;
    app.installTranslator(&translator);
    Theme theme;
    Previewer previewer;
    Overlay overlay;

    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.addImageProvider("gander", new GanderImageProvider);
    engine.addImageProvider("pdf", new PdfImageProvider);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("previewer", &previewer);
    engine.rootContext()->setContextProperty("overlay", &overlay);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (!window)
        return 1;
    overlay.attach(window);

    // The bus name comes last: by the time Nautilus can reach us, the window
    // and every viewer already exist.
    QString error;
    if (!previewer.registerOnBus(&error)) {
        std::fprintf(stderr, "gander: %s\n", qPrintable(error));
        return 1;
    }
    overlay.warmUp([&previewer] { return !previewer.visible(); });
    previewer.trace("ready");

    // For screenshots and checks without a display: GANDER_SNAPSHOT=out.png
    // saves the window each time a preview settles.
    const QString snapshot = qEnvironmentVariable("GANDER_SNAPSHOT");
    QTimer settle;
    if (!snapshot.isEmpty()) {
        settle.setSingleShot(true);
        settle.setInterval(qEnvironmentVariableIntValue("GANDER_SNAPSHOT_DELAY") > 0
                               ? qEnvironmentVariableIntValue("GANDER_SNAPSHOT_DELAY") : 900);
        QObject::connect(&settle, &QTimer::timeout, window, [window, snapshot, &previewer] {
            // The window is transparent; put it on a wallpaper-dark ground so
            // the picture reads the way it does on a desktop.
            const QImage grab = window->grabWindow();
            QImage picture(grab.size(), QImage::Format_RGB32);
            picture.fill(QColor("#3a3f4b"));
            QPainter painter(&picture);
            painter.drawImage(0, 0, grab);
            painter.end();
            picture.save(snapshot);
            previewer.trace("snapshot " + snapshot);
        });
        QObject::connect(&previewer, &Previewer::fileChanged, &settle, qOverload<>(&QTimer::start));
        QObject::connect(&previewer, &Previewer::detailsChanged, &settle, qOverload<>(&QTimer::start));
    }
    return app.exec();
}

int main(int argc, char *argv[]) {
    QStringList arguments;
    for (int i = 1; i < argc; ++i)
        arguments.append(QString::fromLocal8Bit(argv[i]));

    if (arguments.isEmpty() || arguments.first() == "-h" || arguments.first() == "--help") {
        std::fputs(usage, arguments.isEmpty() ? stderr : stdout);
        return arguments.isEmpty() ? 1 : 0;
    }
    if (arguments.first() == "--version" || arguments.first() == "-v") {
        std::printf("gander %s\n", GANDER_VERSION);
        return 0;
    }
    if (arguments.first() == "daemon")
        return runDaemon(argc, argv);
    return runClient(argc, argv, arguments);
}
