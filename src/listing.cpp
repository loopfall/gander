#include "listing.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QLocale>
#include <QMimeDatabase>
#include <QProcess>
#include <QRegularExpression>

#include <algorithm>

namespace Listing {

// Natural order (track 2 before track 10), done by hand: QCollator's numeric
// mode silently does nothing under the C locale, which systemd services and
// minimal systems often run with.
int naturalCompare(const QString &a, const QString &b) {
    qsizetype i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        if (a[i].isDigit() && b[j].isDigit()) {
            qsizetype ei = i, ej = j;
            while (ei < a.size() && a[ei].isDigit())
                ++ei;
            while (ej < b.size() && b[ej].isDigit())
                ++ej;
            // Compare the numbers by value: skip leading zeros, then the
            // longer run is larger, then digit by digit.
            qsizetype si = i, sj = j;
            while (si + 1 < ei && a[si] == '0')
                ++si;
            while (sj + 1 < ej && b[sj] == '0')
                ++sj;
            if (ei - si != ej - sj)
                return ei - si < ej - sj ? -1 : 1;
            for (qsizetype k = 0; k < ei - si; ++k)
                if (a[si + k] != b[sj + k])
                    return a[si + k] < b[sj + k] ? -1 : 1;
            i = ei;
            j = ej;
            continue;
        }
        const QChar ca = a[i].toCaseFolded(), cb = b[j].toCaseFolded();
        if (ca != cb) {
            const int order = QString::localeAwareCompare(QString(ca), QString(cb));
            return order != 0 ? order : (ca < cb ? -1 : 1);
        }
        ++i;
        ++j;
    }
    if (i < a.size() || j < b.size())
        return i < a.size() ? 1 : -1;
    return QString::compare(a, b);
}

static void sortNaturally(QFileInfoList &entries) {
    std::sort(entries.begin(), entries.end(), [](const QFileInfo &a, const QFileInfo &b) {
        if (a.isDir() != b.isDir())
            return a.isDir();
        return naturalCompare(a.fileName(), b.fileName()) < 0;
    });
}

static QString iconFor(const QFileInfo &info) {
    static QMimeDatabase database;
    if (info.isDir())
        return "folder";
    // By name only: opening every file in a big folder to sniff it is exactly
    // the slowness this preview exists to avoid.
    const QMimeType mime = database.mimeTypeForFile(info.fileName(), QMimeDatabase::MatchExtension);
    return mime.iconName() + "," + mime.genericIconName();
}

QVariantMap folder(const QString &path, int maxEntries) {
    QVariantMap map;
    QDir dir(path);
    QFileInfoList entries = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System);
    sortNaturally(entries);

    QVariantList list;
    qint64 bytes = 0;
    int folders = 0;
    for (const auto &entry : entries) {
        if (entry.isDir())
            ++folders;
        else
            bytes += entry.size();
        if (list.size() >= maxEntries)
            continue;
        list.append(QVariantMap{
            {"name", entry.fileName()},
            {"icon", iconFor(entry)},
            {"folder", entry.isDir()},
            {"size", entry.isDir() ? QString() : QLocale().formattedDataSize(entry.size())},
        });
    }
    map["entries"] = list;
    map["count"] = int(entries.size());
    map["folders"] = folders;
    map["files"] = int(entries.size()) - folders;
    map["fileBytes"] = double(bytes);
    if (!dir.isReadable())
        map["error"] = QCoreApplication::translate("gander", "You don't have permission to open this folder.");
    return map;
}

QVariantMap folderSize(const QString &path, int deadlineMs) {
    QElapsedTimer timer;
    timer.start();
    qint64 bytes = 0;
    bool partial = false;
    QDirIterator it(path, QDir::Files | QDir::Hidden | QDir::System | QDir::NoSymLinks, QDirIterator::Subdirectories);
    int seen = 0;
    while (it.hasNext()) {
        it.next();
        bytes += it.fileInfo().size();
        if ((++seen & 255) == 0 && timer.elapsed() > deadlineMs) {
            partial = true;
            break;
        }
    }
    return {{"totalBytes", double(bytes)}, {"totalText", QLocale().formattedDataSize(bytes)}, {"partial", partial}};
}

bool parseArchiveLine(const QString &line, QString *name, qint64 *bytes, bool *folder) {
    // -rw-r--r--  0 user   group    1234 Jan  1  2020 path/to/file
    // drwxr-xr-x  0 user   group       0 Mar 14 09:26 path/to/folder/
    static const QRegularExpression pattern(
        R"(^([\-dlbcps][rwxsStT\-]{9})\S*\s+\d+\s+\S+\s+\S+\s+(\d+)\s+\S+\s+\d+\s+[\d:]+\s+(.+)$)");
    const auto match = pattern.match(line);
    if (!match.hasMatch())
        return false;
    QString entry = match.captured(3);
    const int arrow = entry.indexOf(" -> ");
    if (match.captured(1).startsWith('l') && arrow > 0)
        entry.truncate(arrow);
    *folder = match.captured(1).startsWith('d') || entry.endsWith('/');
    if (entry.endsWith('/'))
        entry.chop(1);
    *name = entry;
    *bytes = match.captured(2).toLongLong();
    return true;
}

QVariantMap archive(const QString &path, int maxEntries) {
    QVariantMap map;
    QProcess bsdtar;
    bsdtar.start("bsdtar", {"-tvf", path});
    if (!bsdtar.waitForStarted(2000)) {
        map["error"] = QCoreApplication::translate("gander", "bsdtar (libarchive) is needed to look inside archives.");
        return map;
    }
    if (!bsdtar.waitForFinished(8000)) {
        bsdtar.kill();
        bsdtar.waitForFinished(500);
        map["partial"] = true;
    }

    static QMimeDatabase database;
    QVariantList list;
    qint64 total = 0;
    int files = 0, folders = 0;
    const QStringList lines = QString::fromUtf8(bsdtar.readAllStandardOutput()).split('\n', Qt::SkipEmptyParts);
    for (const auto &line : lines) {
        QString name;
        qint64 bytes = 0;
        bool isFolder = false;
        if (!parseArchiveLine(line, &name, &bytes, &isFolder))
            continue;
        total += bytes;
        isFolder ? ++folders : ++files;
        if (list.size() >= maxEntries)
            continue;
        const QMimeType mime = database.mimeTypeForFile(name, QMimeDatabase::MatchExtension);
        const int depth = int(name.count('/'));
        list.append(QVariantMap{
            {"name", name.section('/', -1)},
            {"path", name},
            {"depth", depth},
            {"icon", isFolder ? QString("folder") : mime.iconName() + "," + mime.genericIconName()},
            {"folder", isFolder},
            {"size", isFolder ? QString() : QLocale().formattedDataSize(bytes)},
        });
    }
    if (lines.isEmpty() && bsdtar.exitCode() != 0)
        map["error"] = QCoreApplication::translate("gander", "This archive can't be read. It may be damaged or encrypted.");
    map["entries"] = list;
    map["files"] = files;
    map["folders"] = folders;
    map["count"] = files + folders;
    map["totalBytes"] = double(total);
    map["totalText"] = QLocale().formattedDataSize(total);
    return map;
}

QStringList siblings(const QString &directory, bool includeHidden) {
    QDir::Filters filters = QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System;
    if (includeHidden)
        filters |= QDir::Hidden;
    QFileInfoList entries = QDir(directory).entryInfoList(filters);
    sortNaturally(entries);
    QStringList paths;
    paths.reserve(entries.size());
    for (const auto &entry : entries)
        paths.append(entry.absoluteFilePath());
    return paths;
}

} // namespace Listing
