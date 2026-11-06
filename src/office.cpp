#include "office.h"

#include "textloader.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

#include <algorithm>

OfficeConverter::OfficeConverter(const QString &cacheDirectory, QObject *parent)
    : QObject(parent),
      m_cacheDirectory(cacheDirectory.isEmpty()
                           ? QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + "/gander/office"
                           : cacheDirectory) {
    QDir().mkpath(m_cacheDirectory);
}

OfficeConverter::~OfficeConverter() {
    cancel();
}

QString OfficeConverter::executable() {
    for (const char *name : {"soffice", "libreoffice"}) {
        const QString found = QStandardPaths::findExecutable(name);
        if (!found.isEmpty())
            return found;
    }
    return {};
}

QString OfficeConverter::cacheKey(const QFileInfo &info) {
    const QByteArray identity = QFile::encodeName(info.absoluteFilePath()) + '\n'
        + QByteArray::number(info.size()) + '\n'
        + QByteArray::number(info.lastModified().toMSecsSinceEpoch());
    return QString::fromLatin1(QCryptographicHash::hash(identity, QCryptographicHash::Sha1).toHex());
}

QString OfficeConverter::cachedPdf(const QString &path) const {
    const QString pdf = m_cacheDirectory + "/" + cacheKey(QFileInfo(path)) + ".pdf";
    return QFileInfo(pdf).size() > 0 ? pdf : QString();
}

QString OfficeConverter::profileUrl() const {
    return QUrl::fromLocalFile(QFileInfo(m_cacheDirectory).absolutePath() + "/libreoffice-profile").toString();
}

void OfficeConverter::cancel() {
    if (m_process) {
        m_process->disconnect(this);
        m_process->kill();
        m_process->waitForFinished(500);
        m_process->deleteLater();
    }
    m_process = nullptr;
    m_outputDirectory.reset();
    m_done = nullptr;
}

void OfficeConverter::convert(const QString &path, Done done) {
    cancel();
    const QString soffice = executable();
    if (soffice.isEmpty()) {
        done({}, QCoreApplication::translate("gander", "Install LibreOffice to preview documents, spreadsheets and presentations."));
        return;
    }

    m_outputDirectory = std::make_unique<QTemporaryDir>(m_cacheDirectory + "/convert-XXXXXX");
    m_done = std::move(done);
    const QString key = cacheKey(QFileInfo(path));
    const QString outputDirectory = m_outputDirectory->path();

    auto *process = new QProcess(this);
    m_process = process;
    process->setProcessChannelMode(QProcess::MergedChannels);
    connect(process, &QProcess::finished, this, [this, process, path, key, outputDirectory](int code, QProcess::ExitStatus status) {
        if (process != m_process)
            return;
        Done done = std::move(m_done);
        const QString log = QString::fromLocal8Bit(process->readAll()).trimmed();
        process->deleteLater();
        m_process = nullptr;

        // LibreOffice names the output after the input; whatever PDF appeared
        // is the one.
        const QFileInfoList produced = QDir(outputDirectory).entryInfoList({"*.pdf"}, QDir::Files);
        if (status == QProcess::NormalExit && code == 0 && !produced.isEmpty() && produced.first().size() > 0) {
            const QString target = m_cacheDirectory + "/" + key + ".pdf";
            QFile::remove(target);
            if (QFile::rename(produced.first().absoluteFilePath(), target)) {
                m_outputDirectory.reset();
                done(target, {});
                return;
            }
        }
        m_outputDirectory.reset();
        done({}, log.isEmpty() ? QCoreApplication::translate("gander", "LibreOffice couldn't convert this document.")
                               : QCoreApplication::translate("gander", "LibreOffice couldn't convert this document: %1").arg(log.section('\n', -1)));
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || process != m_process)
            return;
        Done done = std::move(m_done);
        process->deleteLater();
        m_process = nullptr;
        m_outputDirectory.reset();
        done({}, QCoreApplication::translate("gander", "LibreOffice failed to start."));
    });

    // A document that takes longer than this is stuck on something (a dialog
    // nobody can see, a broken file); stop it rather than keep a CPU busy.
    QTimer::singleShot(90 * 1000, process, [process] { process->kill(); });

    process->start(soffice, {"-env:UserInstallation=" + profileUrl(), "--headless", "--invisible", "--norestore",
                             "--nologo", "--nodefault", "--nolockcheck", "--convert-to", "pdf", "--outdir",
                             outputDirectory, path});
}

void OfficeConverter::prepare() {
    const QString profile = QUrl(profileUrl()).toLocalFile();
    const QString soffice = executable();
    if (soffice.isEmpty() || QFileInfo::exists(profile))
        return;
    // A throwaway conversion of an empty text file builds the profile. It
    // runs at idle priority, a little after login, and nobody waits for it.
    QTimer::singleShot(45 * 1000, this, [this, soffice] {
        auto *directory = new QTemporaryDir(m_cacheDirectory + "/prepare-XXXXXX");
        QFile seed(directory->filePath("seed.txt"));
        if (!seed.open(QIODevice::WriteOnly)) {
            delete directory;
            return;
        }
        seed.write("gander\n");
        seed.close();
        auto *process = new QProcess(this);
        connect(process, &QProcess::finished, process, [process, directory] {
            process->deleteLater();
            delete directory;
        });
        const QString nice = QStandardPaths::findExecutable("nice");
        QStringList arguments{"-env:UserInstallation=" + profileUrl(), "--headless", "--invisible", "--norestore",
                              "--nologo", "--nodefault", "--nolockcheck", "--convert-to", "pdf", "--outdir",
                              directory->path(), seed.fileName()};
        if (nice.isEmpty())
            process->start(soffice, arguments);
        else
            process->start(nice, QStringList{"-n", "19", soffice} + arguments);
    });
}

void OfficeConverter::prune() {
    const QDateTime cutoff = QDateTime::currentDateTime().addDays(-30);
    QDirIterator it(m_cacheDirectory, {"*.pdf"}, QDir::Files);
    while (it.hasNext()) {
        const QFileInfo info(it.next());
        if (info.lastRead() < cutoff && info.lastModified() < cutoff)
            QFile::remove(info.absoluteFilePath());
    }
    // Leftovers from a conversion that was running when the daemon stopped.
    for (const auto &stale : QDir(m_cacheDirectory).entryInfoList({"convert-*", "prepare-*"}, QDir::Dirs | QDir::NoDotAndDotDot))
        QDir(stale.absoluteFilePath()).removeRecursively();
}

static QByteArray unzip(const QString &archive, const QStringList &members) {
    QProcess bsdtar;
    bsdtar.start("bsdtar", QStringList{"-xOf", archive} + members);
    if (!bsdtar.waitForFinished(4000)) {
        bsdtar.kill();
        bsdtar.waitForFinished(500);
        return {};
    }
    return bsdtar.readAllStandardOutput();
}

QString OfficeConverter::quickText(const QString &path) {
    const QString suffix = QFileInfo(path).suffix().toLower();
    QByteArray xml;
    if (suffix == "docx" || suffix == "docm" || suffix == "dotx") {
        xml = unzip(path, {"word/document.xml"});
    } else if (suffix == "odt" || suffix == "odp" || suffix == "ott" || suffix == "odg") {
        xml = unzip(path, {"content.xml"});
    } else if (suffix == "pptx" || suffix == "pptm") {
        QProcess list;
        list.start("bsdtar", {"-tf", path});
        if (!list.waitForFinished(3000))
            return {};
        QStringList slides;
        static const QRegularExpression slide("^ppt/slides/slide(\\d+)\\.xml$");
        for (const auto &line : QString::fromUtf8(list.readAllStandardOutput()).split('\n'))
            if (slide.match(line.trimmed()).hasMatch())
                slides.append(line.trimmed());
        std::sort(slides.begin(), slides.end(), [](const QString &a, const QString &b) {
            return slide.match(a).captured(1).toInt() < slide.match(b).captured(1).toInt();
        });
        // Each slide is its own XML document; pull them one by one so a
        // broken slide doesn't hide the rest.
        QString text;
        for (const auto &member : slides.mid(0, 60)) {
            const QString part = TextLoader::officeXmlText(unzip(path, {member})).trimmed();
            if (!part.isEmpty())
                text += part + "\n\n";
        }
        return text.trimmed();
    } else {
        return {};
    }
    return TextLoader::officeXmlText(xml).trimmed();
}
