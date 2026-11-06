#pragma once

#include <QHash>
#include <QImage>
#include <QMutex>
#include <QQuickAsyncImageProvider>
#include <QThreadPool>
#include <QVariantMap>

#include <atomic>
#include <memory>

namespace Poppler { class Document; }

// Open PDFs, rendered page by page as QML asks for them. Poppler documents are
// not safe to use from two threads at once, so loading and rendering share a
// single worker thread; pages that scrolled away before their turn are
// cancelled instead of rendered.
class PdfStore {
public:
    static PdfStore &instance();
    QThreadPool *pool() { return &m_pool; }

    // Runs on the pool. Returns docId, pageCount and pageAspects (height over
    // width per page), or error.
    QVariantMap load(const QString &path);
    QImage render(const QString &docId, int page, int widthPixels, const std::atomic_bool &cancelled);

private:
    PdfStore();
    QMutex m_mutex;
    QThreadPool m_pool;
    QHash<QString, std::shared_ptr<Poppler::Document>> m_documents;
    QStringList m_order;
    quint64 m_next = 0;
};

// image://pdf/<docId>/<page>/<width in device pixels>
class PdfImageProvider : public QQuickAsyncImageProvider {
public:
    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;
};
