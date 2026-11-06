#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace TextLoader {

// Reads the head of a text file, up to limit bytes, in a worker thread. The
// map carries text, truncated, lines, language (for the highlighter) and
// longLines, which asks the view to wrap (minified JSON, logs).
QVariantMap load(const QString &path, qint64 limit = 384 * 1024);

// The highlighter family for a file: c, hash, dash, markup, json or plain.
QString languageFor(const QString &fileName, const QString &mime);

// Paragraphs of text from WordprocessingML, OpenDocument or DrawingML XML.
QString officeXmlText(const QByteArray &xml);

// Delimiter-separated values, quoted fields and all. The separator is guessed
// from the first line when none is given, since half of Europe writes ';'.
QList<QStringList> parseDelimited(const QByteArray &data, QChar separator = QChar(), int maxRows = 2000, int maxColumns = 64);
QChar guessSeparator(const QByteArray &firstLine);

} // namespace TextLoader
