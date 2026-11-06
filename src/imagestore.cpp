#include "imagestore.h"

#include <QIcon>
#include <QMutexLocker>
#include <QUrl>

ImageStore &ImageStore::instance() {
    static ImageStore store;
    return store;
}

QString ImageStore::put(const QImage &image) {
    QMutexLocker lock(&m_mutex);
    const QString key = QString::number(++m_next);
    m_images.insert(key, image);
    m_order.append(key);
    while (m_order.size() > 12)
        m_images.remove(m_order.takeFirst());
    return key;
}

QImage ImageStore::get(const QString &key) const {
    QMutexLocker lock(&m_mutex);
    return m_images.value(key);
}

GanderImageProvider::GanderImageProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}

QImage GanderImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize) {
    QImage image;
    if (id.startsWith("store/")) {
        image = ImageStore::instance().get(id.mid(6));
    } else if (id.startsWith("icon/")) {
        // Fall back from the specific icon (text-x-python) to the generic one
        // (text-x-generic) to a blank page, so there is always something.
        // QML percent-encodes the id; names are separated by commas.
        const QStringList names = QUrl::fromPercentEncoding(id.mid(5).toUtf8()).split(',', Qt::SkipEmptyParts);
        QIcon icon;
        for (const auto &name : names + QStringList{"text-x-generic", "application-x-generic", "unknown"}) {
            icon = QIcon::fromTheme(name);
            if (!icon.isNull())
                break;
        }
        const int side = requestedSize.isValid() ? qMax(requestedSize.width(), requestedSize.height()) : 128;
        image = icon.pixmap(QSize(side, side)).toImage();
    }
    if (size)
        *size = image.size();
    return image;
}
