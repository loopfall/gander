#include "audio.h"

#include "imagestore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QtMath>

#include <taglib/audioproperties.h>
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/tvariant.h>

namespace Audio {

static QString toQString(const TagLib::String &value) {
    return QString::fromStdString(value.to8Bit(true)).trimmed();
}

QImage folderCover(const QString &directory) {
    static const QStringList names{"cover", "folder", "front", "album", "albumart", "artwork"};
    static const QStringList suffixes{"jpg", "jpeg", "png", "webp"};
    const QDir dir(directory);
    // Case-insensitive, because Cover.JPG is as common as cover.jpg.
    for (const auto &entry : dir.entryInfoList(QDir::Files | QDir::Readable)) {
        if (names.contains(entry.completeBaseName().toLower()) && suffixes.contains(entry.suffix().toLower())) {
            QImageReader reader(entry.absoluteFilePath());
            reader.setAutoTransform(true);
            const QSize size = reader.size();
            if (size.width() > 1024 || size.height() > 1024)
                reader.setScaledSize(size.scaled(1024, 1024, Qt::KeepAspectRatio));
            const QImage image = reader.read();
            if (!image.isNull())
                return image;
        }
    }
    return {};
}

QColor tint(const QImage &image) {
    if (image.isNull())
        return QColor("#1b1b1f");
    const QImage small = image.scaled(24, 24, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                             .convertToFormat(QImage::Format_RGB32);
    double r = 0, g = 0, b = 0, weight = 0;
    double ar = 0, ag = 0, ab = 0;
    for (int y = 0; y < small.height(); ++y) {
        for (int x = 0; x < small.width(); ++x) {
            const QColor c = QColor::fromRgb(small.pixel(x, y));
            ar += c.redF(); ag += c.greenF(); ab += c.blueF();
            // Saturated, mid-bright pixels carry the character of a cover;
            // greys and near-blacks would turn every backdrop into mud.
            const double w = c.hsvSaturationF() * (1.0 - qAbs(c.valueF() - 0.6));
            r += c.redF() * w; g += c.greenF() * w; b += c.blueF() * w;
            weight += w;
        }
    }
    const int count = small.width() * small.height();
    QColor color = weight > count * 0.05 ? QColor::fromRgbF(r / weight, g / weight, b / weight)
                                         : QColor::fromRgbF(ar / count, ag / count, ab / count);
    float h, s, v;
    color.getHsvF(&h, &s, &v);
    return QColor::fromHsvF(h < 0 ? 0 : h, qMin(s, 0.75f), qBound(0.16f, v * 0.45f, 0.30f));
}

QVariantMap read(const QString &path) {
    QVariantMap map;
    const QByteArray encoded = QFile::encodeName(path);
    TagLib::FileRef file(encoded.constData(), true, TagLib::AudioProperties::Fast);
    QImage cover;

    if (!file.isNull()) {
        if (const TagLib::Tag *tag = file.tag()) {
            map["title"] = toQString(tag->title());
            map["artist"] = toQString(tag->artist());
            map["album"] = toQString(tag->album());
            map["genre"] = toQString(tag->genre());
            if (tag->year())
                map["year"] = int(tag->year());
            if (tag->track())
                map["track"] = int(tag->track());
        }
        if (const TagLib::AudioProperties *properties = file.audioProperties()) {
            map["durationMs"] = properties->lengthInMilliseconds();
            map["bitrate"] = properties->bitrate();
            map["sampleRate"] = properties->sampleRate();
            map["channels"] = properties->channels();
        }
        // The first picture is the front cover in practice; ID3, FLAC, MP4 and
        // Vorbis comments all come through TagLib's PICTURE property.
        const auto pictures = file.complexProperties("PICTURE");
        for (const auto &picture : pictures) {
            const TagLib::ByteVector data = picture.value("data").toByteVector();
            if (data.isEmpty())
                continue;
            cover.loadFromData(reinterpret_cast<const uchar *>(data.data()), int(data.size()));
            if (!cover.isNull())
                break;
        }
    }

    if (cover.isNull())
        cover = folderCover(QFileInfo(path).absolutePath());
    if (!cover.isNull()) {
        if (cover.width() > 1024 || cover.height() > 1024)
            cover = cover.scaled(1024, 1024, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        map["cover"] = "image://gander/store/" + ImageStore::instance().put(cover);
    }
    map["tint"] = tint(cover).name();
    return map;
}

QVector<double> peaks(const QByteArray &pcm, int bars) {
    QVector<double> result(bars, 0.0);
    const auto *samples = reinterpret_cast<const qint16 *>(pcm.constData());
    const qsizetype count = pcm.size() / 2;
    if (count == 0 || bars <= 0)
        return result;
    double loudest = 0;
    for (int bar = 0; bar < bars; ++bar) {
        const qsizetype from = count * bar / bars, to = qMax(from + 1, count * (bar + 1) / bars);
        // RMS rather than the raw peak: one clipped sample shouldn't make a
        // quiet passage look loud.
        double sum = 0;
        for (qsizetype i = from; i < to && i < count; ++i)
            sum += double(samples[i]) * samples[i];
        result[bar] = qSqrt(sum / double(to - from));
        loudest = qMax(loudest, result[bar]);
    }
    if (loudest > 0)
        for (auto &value : result)
            value = qBound(0.04, qPow(value / loudest, 0.8), 1.0);
    return result;
}

} // namespace Audio
