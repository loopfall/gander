#include "textloader.h"

#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QStringDecoder>
#include <QXmlStreamReader>

namespace TextLoader {

QString languageFor(const QString &fileName, const QString &mime) {
    static const QHash<QString, QString> bySuffix = [] {
        QHash<QString, QString> map;
        for (const char *s : {"c", "h", "cc", "cpp", "cxx", "hpp", "hh", "hxx", "m", "mm", "cs", "java", "js", "mjs",
                              "cjs", "jsx", "ts", "tsx", "go", "rs", "swift", "kt", "kts", "dart", "scala", "php",
                              "qml", "css", "scss", "less", "glsl", "frag", "vert", "zig", "groovy", "gradle", "v",
                              "sol", "proto", "jsonc", "json5", "hx", "d", "ino", "cu", "hlsl", "wgsl"})
            map.insert(s, "c");
        for (const char *s : {"py", "pyw", "sh", "bash", "zsh", "fish", "rb", "pl", "pm", "yaml", "yml", "toml", "conf",
                              "cfg", "ini", "cmake", "r", "nix", "ps1", "desktop", "service", "timer", "socket",
                              "env", "gitignore", "dockerignore", "mk", "pro", "pri", "tf", "hcl", "jl", "cr",
                              "nim", "ex", "exs", "coffee", "gd", "awk", "sed", "properties", "editorconfig"})
            map.insert(s, "hash");
        for (const char *s : {"sql", "lua", "hs", "elm", "ada", "adb", "vhd", "vhdl"})
            map.insert(s, "dash");
        for (const char *s : {"html", "htm", "xhtml", "xml", "svg", "ui", "qrc", "plist", "xsl", "xslt", "vue",
                              "svelte", "csproj", "fsproj", "vcxproj", "kml", "gpx", "rss", "atom", "xaml"})
            map.insert(s, "markup");
        for (const char *s : {"json", "geojson", "ipynb", "har", "webmanifest", "lock"})
            map.insert(s, "json");
        return map;
    }();
    static const QHash<QString, QString> byName{
        {"makefile", "hash"}, {"gnumakefile", "hash"}, {"dockerfile", "hash"}, {"containerfile", "hash"},
        {"pkgbuild", "hash"}, {"cmakelists.txt", "hash"}, {".bashrc", "hash"}, {".zshrc", "hash"},
        {".profile", "hash"}, {".bash_profile", "hash"}, {"gemfile", "hash"}, {"rakefile", "hash"},
        {"vagrantfile", "hash"}, {"justfile", "hash"}, {".gitconfig", "hash"}, {"meson.build", "hash"},
    };

    const QString lower = fileName.toLower();
    if (byName.contains(lower))
        return byName.value(lower);
    const QString suffix = QFileInfo(lower).suffix();
    if (bySuffix.contains(suffix))
        return bySuffix.value(suffix);
    if (mime.contains("xml") || mime.contains("html"))
        return "markup";
    if (mime.contains("json"))
        return "json";
    if (mime.contains("shellscript") || mime.contains("python") || mime.contains("ruby") || mime.contains("perl"))
        return "hash";
    if (mime.contains("csrc") || mime.contains("c++") || mime.contains("javascript") || mime.contains("java"))
        return "c";
    return "plain";
}

static QString decode(const QByteArray &bytes) {
    // A BOM settles it; otherwise UTF-8, and Latin-1 for the rare file that
    // isn't (it never fails, and keeps ASCII intact).
    if (auto encoding = QStringConverter::encodingForData(bytes)) {
        QStringDecoder decoder(*encoding);
        return decoder.decode(bytes);
    }
    QStringDecoder utf8(QStringDecoder::Utf8);
    const QString text = utf8.decode(bytes);
    if (!utf8.hasError())
        return text;
    return QString::fromLatin1(bytes);
}

QVariantMap load(const QString &path, qint64 limit) {
    QVariantMap map;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        map["error"] = file.errorString();
        return map;
    }
    QByteArray bytes = file.read(limit + 1);
    const bool truncated = bytes.size() > limit || file.size() > limit;
    if (truncated) {
        bytes.truncate(limit);
        // Cut on a line, and never inside a UTF-8 sequence.
        const qsizetype newline = bytes.lastIndexOf('\n');
        if (newline > limit / 2)
            bytes.truncate(newline + 1);
        while (!bytes.isEmpty() && (static_cast<unsigned char>(bytes.back()) & 0xC0) == 0x80)
            bytes.chop(1);
        if (!bytes.isEmpty() && static_cast<unsigned char>(bytes.back()) >= 0xC0)
            bytes.chop(1);
    }

    QString text = decode(bytes);
    text.replace("\r\n", "\n");
    qsizetype lines = text.count('\n') + (text.endsWith('\n') || text.isEmpty() ? 0 : 1);
    qsizetype longest = 0, current = 0;
    for (const QChar c : text) {
        if (c == '\n') {
            longest = qMax(longest, current);
            current = 0;
        } else {
            ++current;
        }
    }
    longest = qMax(longest, current);

    map["text"] = text;
    map["truncated"] = truncated;
    map["lines"] = double(lines);
    map["longLines"] = longest > 400;
    map["language"] = languageFor(QFileInfo(path).fileName(), {});
    return map;
}

QString officeXmlText(const QByteArray &xml) {
    QXmlStreamReader reader(xml);
    QString out;
    int skip = 0;
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement()) {
            const auto name = reader.name();
            // Field codes and deleted revisions are XML text nobody reads.
            if (name == u"instrText" || name == u"delText" || name == u"tracked-changes")
                ++skip;
            else if (name == u"tab")
                out += '\t';
            else if (name == u"br" || name == u"line-break" || name == u"cr")
                out += '\n';
            else if (name == u"s" && reader.namespaceUri().contains(u"opendocument"))
                out += ' ';
        } else if (reader.isEndElement()) {
            const auto name = reader.name();
            if (name == u"instrText" || name == u"delText" || name == u"tracked-changes")
                skip = qMax(0, skip - 1);
            else if (name == u"p" || name == u"h")
                out += '\n';
        } else if (reader.isCharacters() && skip == 0) {
            out += reader.text();
        }
    }
    static const QRegularExpression gaps("\n{3,}");
    out.replace(gaps, "\n\n");
    return out;
}

QChar guessSeparator(const QByteArray &firstLine) {
    const QList<char> candidates{',', ';', '\t', '|'};
    char best = ',';
    qsizetype most = 0;
    for (char c : candidates) {
        const qsizetype n = firstLine.count(c);
        if (n > most) {
            most = n;
            best = c;
        }
    }
    return QChar(best);
}

QList<QStringList> parseDelimited(const QByteArray &data, QChar separator, int maxRows, int maxColumns) {
    const QString text = decode(data);
    if (separator.isNull())
        separator = guessSeparator(data.left(data.indexOf('\n') < 0 ? data.size() : data.indexOf('\n')));

    QList<QStringList> rows;
    QStringList row;
    QString field;
    bool quoted = false;
    auto endField = [&] {
        if (row.size() < maxColumns)
            row.append(field);
        field.clear();
    };
    for (qsizetype i = 0; i < text.size() && rows.size() < maxRows; ++i) {
        const QChar c = text.at(i);
        if (quoted) {
            if (c == '"') {
                if (i + 1 < text.size() && text.at(i + 1) == '"') {
                    field += '"';
                    ++i;
                } else {
                    quoted = false;
                }
            } else {
                field += c;
            }
        } else if (c == '"' && field.isEmpty()) {
            quoted = true;
        } else if (c == separator) {
            endField();
        } else if (c == '\n' || c == '\r') {
            if (c == '\r' && i + 1 < text.size() && text.at(i + 1) == '\n')
                ++i;
            endField();
            rows.append(row);
            row.clear();
        } else {
            field += c;
        }
    }
    if ((!field.isEmpty() || !row.isEmpty()) && rows.size() < maxRows) {
        endField();
        rows.append(row);
    }
    return rows;
}

} // namespace TextLoader
