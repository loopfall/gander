#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QtTest>

#include "audio.h"
#include "inspector.h"
#include "listing.h"
#include "office.h"
#include "tablemodel.h"
#include "textloader.h"
#include "theme.h"
#include "translator.h"

using Inspector::Kind;

class GanderTests : public QObject {
    Q_OBJECT

private slots:
    void classifiesByMime()
    {
        QCOMPARE(Inspector::classify("inode/directory", {}, false), Kind::Folder);
        QCOMPARE(Inspector::classify("image/jpeg", {}, true), Kind::Image);
        QCOMPARE(Inspector::classify("image/x-canon-cr2", {}, false), Kind::Generic);
        QCOMPARE(Inspector::classify("video/mp4", {}, false), Kind::Video);
        QCOMPARE(Inspector::classify("audio/mpeg", {}, false), Kind::Audio);
        QCOMPARE(Inspector::classify("application/pdf", {}, false), Kind::Pdf);
        QCOMPARE(Inspector::classify("text/csv", {"text/plain"}, false), Kind::Table);
        QCOMPARE(Inspector::classify("text/markdown", {"text/plain"}, false), Kind::Markdown);
        QCOMPARE(Inspector::classify("text/x-python3", {"text/plain"}, false), Kind::Text);
        QCOMPARE(Inspector::classify("application/json", {"application/javascript", "text/plain"}, false), Kind::Text);
        QCOMPARE(Inspector::classify("font/ttf", {}, false), Kind::Font);
        QCOMPARE(Inspector::classify("application/x-compressed-tar", {"application/x-tar"}, false), Kind::Archive);
        QCOMPARE(Inspector::classify("application/octet-stream", {}, false), Kind::Generic);
    }

    void officeBeatsZipAndText()
    {
        // OOXML descends from zip and RTF from text; neither should win.
        QCOMPARE(Inspector::classify("application/vnd.openxmlformats-officedocument.wordprocessingml.document",
                                     {"application/zip"}, false), Kind::Office);
        QCOMPARE(Inspector::classify("application/rtf", {"text/plain"}, false), Kind::Office);
        QCOMPARE(Inspector::classify("application/vnd.oasis.opendocument.spreadsheet", {"application/zip"}, false), Kind::Office);
    }

    void svgIsAnImageNotText()
    {
        QCOMPARE(Inspector::classify("image/svg+xml", {"application/xml", "text/plain"}, true), Kind::Image);
    }

    void escapesUrisLikeGlib()
    {
        // Matches g_filename_to_uri() byte for byte; the thumbnail cache
        // depends on it.
        QCOMPARE(Inspector::fileUri("/a b/č.mp3"), QString("file:///a%20b/%C4%8D.mp3"));
        QCOMPARE(Inspector::fileUri("/x!$&'()*+,-.:=@_~;#?[]%"),
                 QString("file:///x!$&'()*+,-.:=@_~%3B%23%3F%5B%5D%25"));
    }

    void findsFreshThumbnailsOnly()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath("photo.jpg");
        QVERIFY(QDir(dir.path()).mkpath("thumbs/large"));
        const QString name = QCryptographicHash::hash(Inspector::fileUri(file).toUtf8(), QCryptographicHash::Md5).toHex() + ".png";

        QImage thumb(8, 8, QImage::Format_RGB32);
        thumb.fill(Qt::red);
        thumb.setText("Thumb::MTime", "1000");
        QVERIFY(thumb.save(dir.filePath("thumbs/large/" + name)));

        QVERIFY(Inspector::thumbnailFor(file, 1000, dir.filePath("thumbs")).endsWith(name));
        QVERIFY(Inspector::thumbnailFor(file, 2000, dir.filePath("thumbs")).isEmpty());
    }

    void sniffsText()
    {
        QVERIFY(Inspector::looksLikeText("plain words\nand lines\n"));
        QVERIFY(Inspector::looksLikeText("žltý kôň"));
        QVERIFY(!Inspector::looksLikeText(QByteArray("\x7f" "ELF\0\0\0", 7)));
        // A sample cut in the middle of a UTF-8 character is still text.
        QVERIFY(Inspector::looksLikeText(QByteArray("abc\xc5", 4)));
    }

    void inspectsFiles()
    {
        QTemporaryDir dir;
        write(dir.filePath("notes.md"), "# Title\n");
        write(dir.filePath("README"), "no extension, still text\n");
        QImage image(40, 20, QImage::Format_RGB32);
        image.fill(Qt::blue);
        QVERIFY(image.save(dir.filePath("wide.png")));

        QCOMPARE(Inspector::inspect(dir.filePath("notes.md")).value("kind").toString(), QString("markdown"));
        QCOMPARE(Inspector::inspect(dir.filePath("README")).value("kind").toString(), QString("text"));
        const QVariantMap png = Inspector::inspect(dir.filePath("wide.png"));
        QCOMPARE(png.value("kind").toString(), QString("image"));
        QCOMPARE(png.value("width").toInt(), 40);
        QCOMPARE(png.value("height").toInt(), 20);
        QCOMPARE(Inspector::inspect(dir.filePath("gone")).value("kind").toString(), QString("missing"));
        QCOMPARE(Inspector::inspect(dir.path()).value("kind").toString(), QString("folder"));
    }

    void loadsTextWithLimits()
    {
        QTemporaryDir dir;
        QByteArray big;
        for (int i = 0; i < 1000; ++i)
            big += "line " + QByteArray::number(i) + " with ščť\n";
        write(dir.filePath("big.txt"), big);
        const QVariantMap map = TextLoader::load(dir.filePath("big.txt"), 4096);
        QVERIFY(map.value("truncated").toBool());
        QVERIFY(map.value("text").toString().endsWith('\n'));
        QVERIFY(!map.value("text").toString().contains(QChar::ReplacementCharacter));

        write(dir.filePath("min.json"), "{\"a\":\"" + QByteArray(800, 'x') + "\"}");
        QVERIFY(TextLoader::load(dir.filePath("min.json")).value("longLines").toBool());
    }

    void picksHighlighterLanguages()
    {
        QCOMPARE(TextLoader::languageFor("main.cpp", {}), QString("c"));
        QCOMPARE(TextLoader::languageFor("script.py", {}), QString("hash"));
        QCOMPARE(TextLoader::languageFor("Makefile", {}), QString("hash"));
        QCOMPARE(TextLoader::languageFor("query.sql", {}), QString("dash"));
        QCOMPARE(TextLoader::languageFor("page.html", {}), QString("markup"));
        QCOMPARE(TextLoader::languageFor("data.json", {}), QString("json"));
        QCOMPARE(TextLoader::languageFor("notes.txt", "text/plain"), QString("plain"));
    }

    void extractsOfficeText()
    {
        const QByteArray docx =
            "<w:document xmlns:w=\"w\"><w:body>"
            "<w:p><w:r><w:t>Hello</w:t></w:r><w:r><w:t xml:space=\"preserve\"> world</w:t></w:r></w:p>"
            "<w:p><w:r><w:instrText>PAGE</w:instrText><w:t>Second</w:t></w:r></w:p>"
            "</w:body></w:document>";
        QCOMPARE(TextLoader::officeXmlText(docx).trimmed(), QString("Hello world\nSecond"));
    }

    void parsesDelimitedValues()
    {
        const auto rows = TextLoader::parseDelimited("name;city\n\"Smith; John\";\"New \"\"York\"\"\"\r\nAnna;Košice");
        QCOMPARE(rows.size(), 3);
        QCOMPARE(rows[1][0], QString("Smith; John"));
        QCOMPARE(rows[1][1], QString("New \"York\""));
        QCOMPARE(rows[2][1], QString("Košice"));

        const auto multiline = TextLoader::parseDelimited("a,b\n\"one\ntwo\",3\n", QChar(','));
        QCOMPARE(multiline.size(), 2);
        QCOMPARE(multiline[1][0], QString("one\ntwo"));

        TableModel model;
        model.setRows(rows);
        QCOMPARE(model.rowCount(), 3);
        QCOMPARE(model.columnCount(), 2);
        QVERIFY(model.columnWidth(0) >= 56);
    }

    void parsesArchiveListings()
    {
        QString name;
        qint64 bytes = 0;
        bool folder = false;
        QVERIFY(Listing::parseArchiveLine("-rw-r--r--  0 user   group    1234 Jan  1  2020 docs/read me.txt", &name, &bytes, &folder));
        QCOMPARE(name, QString("docs/read me.txt"));
        QCOMPARE(bytes, 1234);
        QVERIFY(!folder);
        QVERIFY(Listing::parseArchiveLine("drwxr-xr-x  0 user   group       0 Mar 14 09:26 docs/", &name, &bytes, &folder));
        QCOMPARE(name, QString("docs"));
        QVERIFY(folder);
        QVERIFY(!Listing::parseArchiveLine("garbage", &name, &bytes, &folder));
    }

    void sortsSiblingsNaturally()
    {
        QTemporaryDir dir;
        for (const char *name : {"track 10.mp3", "track 2.mp3", "Alpha.txt", ".hidden"})
            write(dir.filePath(name), "x");
        QVERIFY(QDir(dir.path()).mkdir("zfolder"));
        QStringList names;
        for (const auto &path : Listing::siblings(dir.path(), false))
            names.append(QFileInfo(path).fileName());
        QCOMPARE(names, QStringList({"zfolder", "Alpha.txt", "track 2.mp3", "track 10.mp3"}));
        QCOMPARE(Listing::siblings(dir.path(), true).size(), 5);
    }

    void cacheKeyFollowsChanges()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("doc.docx");
        write(path, "one");
        const QString first = OfficeConverter::cacheKey(QFileInfo(path));
        write(path, "one two");
        QVERIFY(OfficeConverter::cacheKey(QFileInfo(path)) != first);

        OfficeConverter converter(dir.filePath("cache"));
        QVERIFY(converter.cachedPdf(path).isEmpty());
        write(dir.filePath("cache/" + OfficeConverter::cacheKey(QFileInfo(path)) + ".pdf"), "%PDF");
        QVERIFY(!converter.cachedPdf(path).isEmpty());
    }

    void computesWaveformPeaks()
    {
        QByteArray pcm;
        for (int i = 0; i < 1000; ++i) {
            const qint16 sample = i < 500 ? 1000 : 10000;
            pcm.append(reinterpret_cast<const char *>(&sample), 2);
        }
        const auto bars = Audio::peaks(pcm, 10);
        QCOMPARE(bars.size(), 10);
        QVERIFY(bars[0] < bars[9]);
        QCOMPARE(bars[9], 1.0);
        QCOMPARE(Audio::peaks({}, 4).size(), 4);
    }

    void findsFolderCoverAndTint()
    {
        QTemporaryDir dir;
        QImage cover(32, 32, QImage::Format_RGB32);
        cover.fill(QColor("#e0207a"));
        QVERIFY(cover.save(dir.filePath("Cover.JPG"), "JPG"));
        QVERIFY(!Audio::folderCover(dir.path()).isNull());
        const QColor tint = Audio::tint(cover);
        QVERIFY(tint.valueF() <= 0.31);
        QVERIFY(tint.red() > tint.green());
    }

    void themeFallsBack()
    {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
        QCOMPARE(theme.accentForeground(), QString("black"));
    }

    void themeReadsAccent()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        write(dir.path() + "/theme/colors.toml", "accent = \"#112233\"\n");
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#112233"));
        QCOMPARE(theme.accentForeground(), QString("white"));
    }

    void translatorUsesSlovakPlurals()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "sk\n");
        Translator translator(dir.path() + "/language", QFINDTESTDATA("../src/i18n"));
        QCOMPARE(translator.language(), QString("sk"));
        QCOMPARE(translator.translate("Main", "%n pages", nullptr, 1), QString("%n strana"));
        QCOMPARE(translator.translate("Main", "%n pages", nullptr, 3), QString("%n strany"));
        QCOMPARE(translator.translate("Main", "%n pages", nullptr, 12), QString("%n strán"));
        QCOMPARE(translator.translate("Main", "Close"), QString("Zavrieť"));
    }

    void translatorFallsBackToEnglish()
    {
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("en"));
        QVERIFY(translator.translate("Main", "Close").isNull());
    }

private:
    static void write(const QString &path, const QByteArray &contents)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(contents);
    }
};

QTEST_MAIN(GanderTests)
#include "gander_tests.moc"
