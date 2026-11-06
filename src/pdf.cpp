#include "pdf.h"

#include <QCoreApplication>
#include <QMutexLocker>
#include <QRunnable>

#include <poppler-qt6.h>

PdfStore &PdfStore::instance() {
    static PdfStore store;
    return store;
}

PdfStore::PdfStore() {
    m_pool.setMaxThreadCount(1);
    // Keep the thread alive between files; spawning one per document would
    // add latency to every first page.
    m_pool.setExpiryTimeout(-1);
}

QVariantMap PdfStore::load(const QString &path) {
    QVariantMap map;
    std::shared_ptr<Poppler::Document> document(Poppler::Document::load(path).release());
    if (!document) {
        map["error"] = QCoreApplication::translate("gander", "This PDF can't be opened. It may be damaged.");
        return map;
    }
    if (document->isLocked()) {
        map["error"] = QCoreApplication::translate("gander", "This PDF is protected by a password.");
        return map;
    }
    document->setRenderHint(Poppler::Document::Antialiasing);
    document->setRenderHint(Poppler::Document::TextAntialiasing);
    document->setRenderHint(Poppler::Document::TextHinting);
    document->setRenderHint(Poppler::Document::TextSlightHinting);
    document->setRenderHint(Poppler::Document::ThinLineShape);

    const int count = document->numPages();
    QVariantList aspects;
    // Every page's shape lets the list reserve the right height before the
    // page renders, so the scrollbar never jumps. Very long documents use the
    // first page's shape for the rest instead of opening a thousand pages.
    const int measured = qMin(count, 400);
    double last = 1.414;
    for (int i = 0; i < count; ++i) {
        if (i < measured) {
            if (auto page = document->page(i)) {
                const QSizeF size = page->pageSizeF();
                if (size.width() > 0)
                    last = size.height() / size.width();
            }
        }
        aspects.append(last);
    }

    QString id;
    {
        QMutexLocker lock(&m_mutex);
        id = QString::number(++m_next);
        m_documents.insert(id, document);
        m_order.append(id);
        while (m_order.size() > 3)
            m_documents.remove(m_order.takeFirst());
    }
    map["docId"] = id;
    map["pageCount"] = count;
    map["pageAspects"] = aspects;
    const QString title = document->info("Title").trimmed();
    if (!title.isEmpty())
        map["documentTitle"] = title;
    return map;
}

QImage PdfStore::render(const QString &docId, int page, int widthPixels, const std::atomic_bool &cancelled) {
    std::shared_ptr<Poppler::Document> document;
    {
        QMutexLocker lock(&m_mutex);
        document = m_documents.value(docId);
    }
    if (!document || cancelled)
        return {};
    auto pdfPage = document->page(page);
    if (!pdfPage)
        return {};
    const QSizeF size = pdfPage->pageSizeF();
    if (size.width() <= 0)
        return {};
    const double dpi = 72.0 * qBound(64, widthPixels, 6000) / size.width();
    return pdfPage->renderToImage(dpi, dpi);
}

namespace {

class PdfResponse : public QQuickImageResponse, public QRunnable {
public:
    PdfResponse(const QString &docId, int page, int width) : m_docId(docId), m_page(page), m_width(width) {
        setAutoDelete(false);
    }

    QQuickTextureFactory *textureFactory() const override {
        return QQuickTextureFactory::textureFactoryForImage(m_image);
    }

    void cancel() override { m_cancelled = true; }

    void run() override {
        if (!m_cancelled)
            m_image = PdfStore::instance().render(m_docId, m_page, m_width, m_cancelled);
        emit finished();
    }

private:
    QString m_docId;
    int m_page;
    int m_width;
    QImage m_image;
    std::atomic_bool m_cancelled{false};
};

} // namespace

QQuickImageResponse *PdfImageProvider::requestImageResponse(const QString &id, const QSize &) {
    const QStringList parts = id.split('/');
    auto *response = new PdfResponse(parts.value(0), parts.value(1).toInt(), parts.value(2).toInt());
    PdfStore::instance().pool()->start(response);
    return response;
}
