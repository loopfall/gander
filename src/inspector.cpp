#include "inspector.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QLocale>
#include <QMimeDatabase>
#include <QProcess>
#include <QSet>
#include <QStandardPaths>
#include <QStringDecoder>
#include <QUrl>

namespace Inspector {

QString kindName(Kind kind) {
    switch (kind) {
    case Kind::Missing: return "missing";
    case Kind::Folder: return "folder";
    case Kind::Image: return "image";
    case Kind::Animated: return "animated";
    case Kind::Video: return "video";
    case Kind::Audio: return "audio";
    case Kind::Pdf: return "pdf";
    case Kind::Office: return "office";
    case Kind::Table: return "table";
    case Kind::Markdown: return "markdown";
    case Kind::Text: return "text";
    case Kind::Font: return "font";
    case Kind::Archive: return "archive";
    case Kind::Generic: return "generic";
    }
    return "generic";
}

static bool isOffice(const QString &mime) {
    static const QSet<QString> exact{
        "application/msword", "application/rtf", "text/rtf", "application/vnd.visio",
        "application/x-abiword", "application/vnd.wordperfect", "application/x-mswrite",
        "application/vnd.apple.pages", "application/vnd.apple.numbers", "application/vnd.apple.keynote",
    };
    return exact.contains(mime)
        || mime.startsWith("application/vnd.openxmlformats-officedocument.")
        || mime.startsWith("application/vnd.oasis.opendocument.")
        || mime.startsWith("application/vnd.ms-word")
        || mime.startsWith("application/vnd.ms-excel")
        || mime.startsWith("application/vnd.ms-powerpoint");
}

static bool isArchive(const QString &mime, const QStringList &ancestors) {
    static const QSet<QString> archives{
        "application/zip", "application/x-tar", "application/x-compressed-tar",
        "application/x-bzip-compressed-tar", "application/x-bzip2-compressed-tar",
        "application/x-xz-compressed-tar", "application/x-zstd-compressed-tar",
        "application/x-lzma-compressed-tar", "application/x-lz4-compressed-tar",
        "application/x-7z-compressed", "application/vnd.rar", "application/x-rar",
        "application/x-rar-compressed", "application/java-archive", "application/x-cpio",
        "application/vnd.debian.binary-package", "application/x-rpm", "application/x-archive",
        "application/x-iso9660-image", "application/x-xar", "application/x-cd-image",
        "application/vnd.android.package-archive", "application/x-arch-pkg",
    };
    if (archives.contains(mime))
        return true;
    for (const auto &ancestor : ancestors)
        if (archives.contains(ancestor))
            return true;
    return false;
}

Kind classify(const QString &mime, const QStringList &ancestors, bool imageReadable) {
    if (mime == "inode/directory")
        return Kind::Folder;
    // Office formats come first: OOXML and ODF descend from application/zip,
    // and RTF from text/plain, but nobody wants to see those as archives or
    // markup.
    if (isOffice(mime))
        return Kind::Office;
    if (mime == "application/pdf" || ancestors.contains("application/pdf"))
        return Kind::Pdf;
    if (mime == "text/csv" || mime == "text/x-csv" || mime == "text/tab-separated-values")
        return Kind::Table;
    if (mime == "text/markdown" || mime == "text/x-markdown")
        return Kind::Markdown;
    // Before text: SVG descends from XML, which descends from text/plain.
    if (mime.startsWith("image/"))
        return imageReadable ? Kind::Image : Kind::Generic;
    if (mime.startsWith("video/") || mime == "application/vnd.rn-realmedia" || mime == "application/x-matroska")
        return Kind::Video;
    if (mime.startsWith("audio/") || mime == "application/ogg" || mime == "application/x-ogg")
        return Kind::Audio;
    if (mime.startsWith("font/") || mime.startsWith("application/x-font") || mime == "application/vnd.ms-opentype")
        return Kind::Font;
    if (isArchive(mime, ancestors))
        return Kind::Archive;
    if (mime == "text/plain" || mime.startsWith("text/") || ancestors.contains("text/plain") || mime == "application/x-zerosize")
        return Kind::Text;
    return Kind::Generic;
}

QString fileUri(const QString &absolutePath) {
    static const char hex[] = "0123456789ABCDEF";
    static const QByteArray allowed = "!$&'()*+,-./:=@_~";
    QByteArray out = "file://";
    for (const char ch : QFile::encodeName(absolutePath)) {
        const auto c = static_cast<unsigned char>(ch);
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || allowed.contains(ch)) {
            out += ch;
        } else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 15];
        }
    }
    return QString::fromLatin1(out);
}

// The Thumb::MTime text chunk of a thumbnail PNG. Read by hand: Qt's reader
// splits keys at colons and loses this one. The chunk sits before the image
// data, so only the first few hundred bytes are read.
static QString thumbnailStamp(const QString &png) {
    QFile file(png);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const QByteArray head = file.read(4096);
    qsizetype at = 8;
    while (at + 8 <= head.size()) {
        const quint32 length = (quint32(uchar(head[at])) << 24) | (quint32(uchar(head[at + 1])) << 16)
                             | (quint32(uchar(head[at + 2])) << 8) | quint32(uchar(head[at + 3]));
        const QByteArray type = head.mid(at + 4, 4);
        if (type == "IDAT" || at + 8 + qsizetype(length) > head.size())
            break;
        if (type == "tEXt") {
            const QByteArray chunk = head.mid(at + 8, length);
            const qsizetype nul = chunk.indexOf('\0');
            if (nul > 0 && chunk.left(nul) == "Thumb::MTime")
                return QString::fromLatin1(chunk.mid(nul + 1));
        }
        at += 12 + length;
    }
    return {};
}

QString thumbnailFor(const QString &absolutePath, qint64 mtime, const QString &cacheRoot) {
    const QString root = cacheRoot.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + "/thumbnails"
        : cacheRoot;
    const QString name = QString::fromLatin1(
        QCryptographicHash::hash(fileUri(absolutePath).toUtf8(), QCryptographicHash::Md5).toHex()) + ".png";
    for (const char *tier : {"xx-large", "x-large", "large", "normal"}) {
        const QString candidate = root + "/" + tier + "/" + name;
        if (!QFileInfo::exists(candidate))
            continue;
        const QString stamp = thumbnailStamp(candidate);
        if (stamp.isEmpty() || stamp == QString::number(mtime))
            return QUrl::fromLocalFile(candidate).toString();
    }
    return {};
}

QString localPathForUri(const QString &uri) {
    const QUrl url(uri);
    if (url.isLocalFile())
        return url.toLocalFile();
    if (url.scheme().isEmpty())
        return uri;

    QProcess gio;
    gio.start("gio", {"info", "-a", "standard::target-uri", uri});
    if (!gio.waitForFinished(2000)) {
        gio.kill();
        return {};
    }
    for (const auto &line : QString::fromUtf8(gio.readAllStandardOutput()).split('\n')) {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith("local path:"))
            return trimmed.mid(11).trimmed();
        if (trimmed.startsWith("standard::target-uri:")) {
            const QUrl target(trimmed.mid(21).trimmed());
            if (target.isLocalFile())
                return target.toLocalFile();
        }
    }
    return {};
}

bool looksLikeText(const QByteArray &head) {
    if (head.contains('\0'))
        return false;
    QStringDecoder decoder(QStringDecoder::Utf8);
    // A multibyte sequence cut off at the end of the sample is still text.
    QByteArray sample = head;
    while (!sample.isEmpty() && (static_cast<unsigned char>(sample.back()) & 0xC0) == 0x80)
        sample.chop(1);
    if (!sample.isEmpty() && static_cast<unsigned char>(sample.back()) >= 0xC0)
        sample.chop(1);
    (void)decoder.decode(sample);
    return !decoder.hasError();
}

static const QSet<QString> &readableImageTypes() {
    static const QSet<QString> types = [] {
        QSet<QString> set;
        for (const auto &type : QImageReader::supportedMimeTypes())
            set.insert(QString::fromLatin1(type));
        return set;
    }();
    return types;
}

QVariantMap inspect(const QString &path) {
    QVariantMap map;
    const QFileInfo info(path);
    const QString absolute = info.absoluteFilePath();
    map["path"] = absolute;
    map["url"] = QUrl::fromLocalFile(absolute).toString();
    map["name"] = info.fileName().isEmpty() ? absolute : info.fileName();
    map["dir"] = info.absolutePath();

    if (!info.exists()) {
        map["kind"] = kindName(Kind::Missing);
        map["iconName"] = "dialog-question";
        map["error"] = QCoreApplication::translate("gander", "This file no longer exists.");
        return map;
    }

    static QMimeDatabase database;
    QMimeType mime = database.mimeTypeForFile(info);
    if (mime.name() == "application/octet-stream" && info.size() < 16 * 1024 * 1024) {
        QFile file(absolute);
        if (file.open(QIODevice::ReadOnly) && looksLikeText(file.read(8192)))
            mime = database.mimeTypeForName("text/plain");
    }

    const QString mimeName = mime.name();
    Kind kind = classify(mimeName, mime.allAncestors(), readableImageTypes().contains(mimeName));
    const qint64 mtime = info.lastModified().toSecsSinceEpoch();

    map["mime"] = mimeName;
    map["mimeComment"] = mime.comment();
    map["iconName"] = mime.iconName();
    map["genericIconName"] = mime.genericIconName();
    map["size"] = double(info.size());
    map["sizeText"] = info.isDir() ? QString() : QLocale().formattedDataSize(info.size());
    map["modified"] = QLocale().toString(info.lastModified(), QLocale::ShortFormat);
    map["suffix"] = info.suffix().toLower();
    map["readable"] = info.isReadable();
    map["hidden"] = info.isHidden();

    if (kind == Kind::Image) {
        // The header alone gives the size, so the window takes its final shape
        // before a single pixel is decoded. EXIF rotation swaps the sides.
        QImageReader reader(absolute);
        reader.setAutoTransform(true);
        QSize size = reader.size();
        if (reader.transformation() & QImageIOHandler::TransformationRotate90)
            size.transpose();
        map["width"] = size.width();
        map["height"] = size.height();
        if (reader.supportsAnimation() && reader.imageCount() > 1)
            kind = Kind::Animated;
    }

    if (kind != Kind::Folder)
        map["thumbnail"] = thumbnailFor(absolute, mtime);
    if (!info.isReadable())
        map["error"] = QCoreApplication::translate("gander", "You don't have permission to read this file.");

    map["kind"] = kindName(kind);
    return map;
}

} // namespace Inspector
