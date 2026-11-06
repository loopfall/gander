#pragma once

#include <QHash>
#include <QImage>
#include <QMutex>
#include <QQuickImageProvider>
#include <QStringList>

// Images made in worker threads (cover art) handed to QML by key, as
// image://gander/store/<key>. Only the last few are kept.
class ImageStore {
public:
    static ImageStore &instance();
    QString put(const QImage &image);
    QImage get(const QString &key) const;

private:
    mutable QMutex m_mutex;
    QHash<QString, QImage> m_images;
    QStringList m_order;
    quint64 m_next = 0;
};

// image://gander/store/<key> and image://gander/icon/<theme icon name>.
class GanderImageProvider : public QQuickImageProvider {
public:
    GanderImageProvider();
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
};
