#pragma once

#include <QColor>
#include <QImage>
#include <QVariantMap>
#include <QVector>

namespace Audio {

// Tags, stream properties and cover art in one pass with TagLib, which reads
// only the headers, so a 300 MB FLAC costs the same as a 3 MB MP3. Cover art
// comes from the file first and from cover.jpg or folder.png beside it second.
// The map carries "cover" as an image://gander/store URL and "tint", a dark
// colour drawn from the cover for the backdrop.
QVariantMap read(const QString &path);

QImage folderCover(const QString &directory);

// A colour that represents the artwork: the average of its saturated pixels,
// darkened enough for white text to read on it.
QColor tint(const QImage &image);

// Peaks for a waveform from raw signed 16-bit mono samples, normalized to
// 0..1 and spread over the given number of bars.
QVector<double> peaks(const QByteArray &pcm, int bars);

} // namespace Audio
