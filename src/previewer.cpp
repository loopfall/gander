#include "previewer.h"

#include "audio.h"
#include "dbusadaptor.h"
#include "highlighter.h"
#include "inspector.h"
#include "listing.h"
#include "pdf.h"
#include "textloader.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QLocale>
#include <QMimeDatabase>
#include <QMutex>
#include <QQuickTextDocument>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QtConcurrent>

namespace {

// "Open with Loupe": the default application's name for a MIME type, from
// xdg-mime and the .desktop file it names, localized like the launcher.
QString defaultApplication(const QString &mime) {
    static QMutex mutex;
    static QHash<QString, QString> cache;
    {
        QMutexLocker lock(&mutex);
        if (cache.contains(mime))
            return cache.value(mime);
    }
    QString name;
    QProcess xdgMime;
    xdgMime.start("xdg-mime", {"query", "default", mime});
    if (xdgMime.waitForFinished(1500)) {
        const QString desktopId = QString::fromUtf8(xdgMime.readAllStandardOutput()).trimmed();
        const QString desktopFile = desktopId.isEmpty()
            ? QString() : QStandardPaths::locate(QStandardPaths::ApplicationsLocation, desktopId);
        QFile file(desktopFile);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QString language = QLocale().name();                 // sk_SK
            const QString shortLanguage = language.section('_', 0, 0); // sk
            QString plain, localized, localizedShort;
            bool inEntry = false;
            while (!file.atEnd()) {
                const QString line = QString::fromUtf8(file.readLine()).trimmed();
                if (line.startsWith('[')) {
                    inEntry = line == "[Desktop Entry]";
                    continue;
                }
                if (!inEntry)
                    continue;
                if (line.startsWith("Name="))
                    plain = line.mid(5);
                else if (line.startsWith("Name[" + language + "]="))
                    localized = line.section('=', 1);
                else if (line.startsWith("Name[" + shortLanguage + "]="))
                    localizedShort = line.section('=', 1);
            }
            name = !localized.isEmpty() ? localized : !localizedShort.isEmpty() ? localizedShort : plain;
        }
    } else {
        xdgMime.kill();
        xdgMime.waitForFinished(200);
    }
    QMutexLocker lock(&mutex);
    cache.insert(mime, name);
    return name;
}

// Children started from a systemd service belong to its cgroup and die with
// it; a scope of their own keeps the opened app alive when gander restarts.
void launchDetached(const QStringList &command) {
    const QString systemdRun = QStandardPaths::findExecutable("systemd-run");
    if (!systemdRun.isEmpty() && !qEnvironmentVariableIsEmpty("INVOCATION_ID")) {
        QProcess::startDetached(systemdRun, QStringList{"--user", "--scope", "--quiet", "--collect", "--"} + command);
        return;
    }
    QProcess::startDetached(command.first(), command.mid(1));
}

} // namespace

Previewer::Previewer(QObject *parent) : QObject(parent) {
    m_tracing = !qEnvironmentVariableIsEmpty("GANDER_TRACE");
    m_clock.start();
    new NautilusPreviewer2Adaptor(this);
    new NautilusPreviewerAdaptor(this);
    m_office.prune();
    m_office.prepare();
}

Previewer::~Previewer() {
    stopProcesses();
}

bool Previewer::registerOnBus(QString *error) {
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        *error = QCoreApplication::translate("gander", "No D-Bus session bus.");
        return false;
    }
    if (!bus.registerObject(objectPath(), this, QDBusConnection::ExportAdaptors)) {
        *error = bus.lastError().message();
        return false;
    }
    // Another previewer (Sushi, or a second gander) may hold the name; ask it
    // to step aside, and let a later gander replace this one the same way.
    const auto reply = bus.interface()->registerService(serviceName(), QDBusConnectionInterface::ReplaceExistingService,
                                                        QDBusConnectionInterface::AllowReplacement);
    if (!reply.isValid() || reply.value() != QDBusConnectionInterface::ServiceRegistered) {
        *error = QCoreApplication::translate("gander", "Another previewer owns %1 and won't let go. Close it (for example Sushi) and start gander again.")
                     .arg(serviceName());
        return false;
    }
    connect(bus.interface(), &QDBusConnectionInterface::serviceUnregistered, this, [](const QString &name) {
        if (name == serviceName())
            QCoreApplication::quit();
    });
    return true;
}

void Previewer::trace(const QString &what) {
    if (m_tracing)
        qInfo("gander %6.1f ms  %s", m_clock.nsecsElapsed() / 1e6, qPrintable(what));
}

void Previewer::showUri(const QString &uri, const QString &windowHandle, bool closeIfShown) {
    m_clock.restart();
    trace("ShowFile " + uri);
    const QString path = Inspector::localPathForUri(uri);
    if (m_visible && closeIfShown && path == m_file.value("path").toString()) {
        close();
        return;
    }
    m_fromFileManager = windowHandle != "gander-cli";
    if (windowHandle != m_parentHandle && windowHandle != "gander-cli") {
        m_parentHandle = windowHandle;
        emit parentHandleChanged();
    }
    if (path.isEmpty()) {
        QVariantMap missing{{"name", uri}, {"kind", "missing"}, {"iconName", "dialog-question"},
                            {"error", QCoreApplication::translate("gander", "This location can't be previewed.")}};
        ++m_generation;
        m_file = missing;
        m_details.clear();
        emit fileChanged();
        emit detailsChanged();
        setVisible(true);
        return;
    }
    show(path);
}

void Previewer::show(const QString &path) {
    const bool stepping = m_visible;
    ++m_generation;
    stopProcesses();
    m_file = Inspector::inspect(path);
    m_details.clear();
    trace("inspected " + m_file.value("kind").toString());
    emit fileChanged();
    emit detailsChanged();
    setVisible(true);
    startLoading();
    loadSiblings(stepping);
}

void Previewer::close() {
    if (!m_visible)
        return;
    ++m_generation;
    stopProcesses();
    setVisible(false);
}

void Previewer::setVisible(bool visible) {
    if (m_visible == visible)
        return;
    m_visible = visible;
    emit visibleChanged();
}

void Previewer::merge(const QVariantMap &values) {
    for (auto it = values.begin(); it != values.end(); ++it)
        m_details.insert(it.key(), it.value());
    emit detailsChanged();
}

template <typename Function> void Previewer::runAsync(Function function) {
    const quint64 generation = m_generation;
    auto *watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcher<QVariantMap>::finished, this, [this, watcher, generation] {
        if (generation == m_generation)
            merge(watcher->result());
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run(function));
}

void Previewer::stopProcesses() {
    if (m_waveform) {
        m_waveform->disconnect(this);
        m_waveform->kill();
        m_waveform->deleteLater();
    }
    m_waveform = nullptr;
}

void Previewer::startLoading() {
    const QString kind = m_file.value("kind").toString();
    const QString path = m_file.value("path").toString();
    const QString mime = m_file.value("mime").toString();
    if (kind == "missing" || !m_file.value("readable", true).toBool())
        return;

    if (kind != "folder" && !mime.isEmpty())
        runAsync([mime] { return QVariantMap{{"openWith", defaultApplication(mime)}}; });

    if (kind == "folder") {
        runAsync([path] { return Listing::folder(path); });
        runAsync([path] { return Listing::folderSize(path); });
    } else if (kind == "audio") {
        runAsync([path] { return Audio::read(path); });
        startWaveform(path);
    } else if (kind == "pdf") {
        loadPdf(path);
    } else if (kind == "office") {
        const QString cached = m_office.cachedPdf(path);
        if (!cached.isEmpty()) {
            // Mark it as used so pruning keeps it.
            QFile touched(cached);
            if (touched.open(QIODevice::ReadWrite))
                touched.setFileTime(QDateTime::currentDateTime(), QFileDevice::FileAccessTime);
            loadPdf(cached);
            return;
        }
        merge({{"converting", true}});
        runAsync([path] { return QVariantMap{{"quickText", OfficeConverter::quickText(path)}}; });
        const quint64 generation = m_generation;
        m_office.convert(path, [this, generation](const QString &pdf, const QString &error) {
            if (generation != m_generation)
                return;
            if (!error.isEmpty()) {
                merge({{"converting", false}, {"error", error}});
                return;
            }
            loadPdf(pdf);
        });
    } else if (kind == "table") {
        const quint64 generation = m_generation;
        auto *watcher = new QFutureWatcher<QList<QStringList>>(this);
        connect(watcher, &QFutureWatcher<QList<QStringList>>::finished, this, [this, watcher, generation] {
            if (generation == m_generation) {
                const auto rows = watcher->result();
                m_table.setRows(rows);
                merge({{"rows", int(rows.size())}, {"columns", m_table.columnCount()}});
            }
            watcher->deleteLater();
        });
        const bool tabs = mime == "text/tab-separated-values" || m_file.value("suffix").toString() == "tsv";
        watcher->setFuture(QtConcurrent::run([path, tabs] {
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly))
                return QList<QStringList>();
            return TextLoader::parseDelimited(file.read(4 * 1024 * 1024), tabs ? QChar('\t') : QChar());
        }));
    } else if (kind == "text" || kind == "markdown") {
        runAsync([path, mime] {
            QVariantMap map = TextLoader::load(path);
            map["language"] = TextLoader::languageFor(QFileInfo(path).fileName(), mime);
            return map;
        });
    } else if (kind == "font") {
        if (m_fontId >= 0)
            QFontDatabase::removeApplicationFont(m_fontId);
        m_fontId = QFontDatabase::addApplicationFont(path);
        const QStringList families = m_fontId >= 0 ? QFontDatabase::applicationFontFamilies(m_fontId) : QStringList();
        if (families.isEmpty())
            merge({{"error", QCoreApplication::translate("gander", "This font can't be loaded.")}});
        else
            merge({{"fontFamily", families.first()}, {"fontFamilies", families}});
    } else if (kind == "archive") {
        runAsync([path] { return Listing::archive(path); });
    }
}

void Previewer::loadPdf(const QString &pdf) {
    const quint64 generation = m_generation;
    auto *watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcher<QVariantMap>::finished, this, [this, watcher, generation] {
        if (generation == m_generation) {
            QVariantMap result = watcher->result();
            result["converting"] = false;
            merge(result);
            trace("pdf loaded");
        }
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run(PdfStore::instance().pool(), [pdf] { return PdfStore::instance().load(pdf); }));
}

void Previewer::startWaveform(const QString &path) {
    const QString ffmpeg = QStandardPaths::findExecutable("ffmpeg");
    if (ffmpeg.isEmpty())
        return;
    const quint64 generation = m_generation;
    auto *process = new QProcess(this);
    m_waveform = process;
    // 1 kHz mono is plenty for a few hundred bars and keeps a long podcast
    // to a few megabytes of samples; the decode runs at idle priority.
    connect(process, &QProcess::finished, this, [this, process, generation](int code) {
        const QByteArray pcm = process->readAllStandardOutput();
        process->deleteLater();
        if (process == m_waveform)
            m_waveform = nullptr;
        if (generation != m_generation || code != 0 || pcm.isEmpty())
            return;
        QVariantList bars;
        for (double value : Audio::peaks(pcm, 120))
            bars.append(value);
        merge({{"waveform", bars}});
    });
    process->start(ffmpeg, {"-nostdin", "-v", "error", "-i", path, "-vn", "-ac", "1", "-ar", "1000", "-t", "7200",
                            "-f", "s16le", "pipe:1"});
}

void Previewer::loadSiblings(bool stepping) {
    const QString directory = m_file.value("dir").toString();
    const QString path = m_file.value("path").toString();
    if (directory.isEmpty())
        return;
    const bool hidden = m_file.value("hidden").toBool();
    // The same folder keeps its list while stepping through it; it's read
    // again when the preview opens fresh, which catches files added since.
    if (stepping && directory == m_siblingsDirectory && !m_siblings.isEmpty() && m_siblings.contains(path)) {
        m_index = int(m_siblings.indexOf(path));
        emit siblingsChanged();
        updatePrefetch();
        return;
    }
    const quint64 generation = m_generation;
    auto *watcher = new QFutureWatcher<QStringList>(this);
    connect(watcher, &QFutureWatcher<QStringList>::finished, this, [this, watcher, generation, directory, path] {
        if (generation == m_generation) {
            m_siblings = watcher->result();
            m_siblingsDirectory = directory;
            m_index = int(m_siblings.indexOf(path));
            emit siblingsChanged();
            updatePrefetch();
        }
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run([directory, hidden] { return Listing::siblings(directory, hidden); }));
}

void Previewer::updatePrefetch() {
    // The neighbours a press of an arrow key is most likely to land on are
    // decoded ahead of time into the image cache. Only images: they are the
    // ones people flick through, and the decode is the slow part.
    static QMimeDatabase database;
    QStringList urls;
    for (int offset : {1, -1, 2}) {
        const int i = m_index + offset;
        if (m_index < 0 || i < 0 || i >= m_siblings.size())
            continue;
        const QString &candidate = m_siblings.at(i);
        const QMimeType mime = database.mimeTypeForFile(candidate, QMimeDatabase::MatchExtension);
        if (mime.name().startsWith("image/") && !mime.name().contains("svg") && QFileInfo(candidate).size() < 80 * 1024 * 1024)
            urls.append(QUrl::fromLocalFile(candidate).toString());
    }
    if (urls != m_prefetch) {
        m_prefetch = urls;
        emit prefetchChanged();
    }
}

void Previewer::navigate(int direction) {
    if (m_fromFileManager) {
        // Nautilus owns the order (its sort, its view); it moves the
        // selection and sends the new file back.
        emit selectionEvent(uint(direction));
        return;
    }
    if (m_siblings.isEmpty() || m_index < 0)
        return;
    const int step = (direction == 2 || direction == 4) ? -1 : 1;
    const int next = m_index + step;
    if (next < 0 || next >= m_siblings.size())
        return;
    m_clock.restart();
    show(m_siblings.at(next));
}

void Previewer::openExternally() {
    const QString path = m_file.value("path").toString();
    if (path.isEmpty())
        return;
    launchDetached({"gio", "open", path});
    close();
}

void Previewer::trash() {
    if (!m_fromFileManager)
        return;
    const QString uri = Inspector::fileUri(m_file.value("path").toString());
    close();
    emit trashRequested(uri);
}

void Previewer::rename() {
    if (!m_fromFileManager)
        return;
    const QString uri = Inspector::fileUri(m_file.value("path").toString());
    close();
    // Nautilus ignores its rename popover while focus is still coming back
    // from the overlay; Sushi waits the same way.
    QTimer::singleShot(200, this, [this, uri] { emit renameRequested(uri); });
}

void Previewer::highlight(QQuickTextDocument *document, const QString &language, const QString &accent) {
    QTextDocument *target = document ? document->textDocument() : nullptr;
    // A highlighter already on this document follows its text by itself.
    // Adding or removing one reformats the document, which QML reports as a
    // text change, so re-entry is ignored too.
    if (m_highlighting || (m_highlighter && m_highlighter->document() == target && m_highlighterLanguage == language))
        return;
    m_highlighting = true;
    delete m_highlighter;
    m_highlighter = nullptr;
    m_highlighterLanguage.clear();
    // Highlighting runs on the GUI thread in one go; past this size it would
    // cost more than it gives.
    if (target && language != "plain" && target->characterCount() <= 160 * 1024) {
        m_highlighter = new Highlighter(target, language, QColor(accent));
        m_highlighterLanguage = language;
    }
    m_highlighting = false;
}

void Previewer::polishMarkdown(QQuickTextDocument *document) {
    QTextDocument *target = document ? document->textDocument() : nullptr;
    if (!target)
        return;
    // Qt's Markdown import sets no paragraph spacing at all; give the text
    // the rhythm of a rendered README: air between paragraphs, more above
    // headings, and code blocks on a tinted ground.
    QTextCursor cursor(target);
    cursor.beginEditBlock();
    for (QTextBlock block = target->begin(); block.isValid(); block = block.next()) {
        QTextBlockFormat format = block.blockFormat();
        const bool code = format.nonBreakableLines() || format.hasProperty(QTextFormat::BlockCodeFence);
        const int heading = format.headingLevel();
        const bool listItem = block.textList() != nullptr;
        format.setBottomMargin(code || listItem ? 2 : 10);
        if (heading > 0) {
            format.setTopMargin(heading <= 2 ? 22 : 14);
            format.setBottomMargin(8);
        }
        if (code)
            format.setBackground(QColor("#1f1f25"));
        format.setLineHeight(code ? 100 : 125, QTextBlockFormat::ProportionalHeight);
        cursor.setPosition(block.position());
        cursor.setBlockFormat(format);
    }
    cursor.endEditBlock();
}
