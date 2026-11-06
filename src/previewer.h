#pragma once

#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QStringList>
#include <QVariantMap>

#include "office.h"
#include "tablemodel.h"

class QQuickTextDocument;
class Highlighter;

// What is being previewed and everything known about it. `file` is the fast,
// synchronous first look (enough for the first frame); `details` fills in as
// worker threads finish (tags, pages, text, listings). Every request carries a
// generation number, so a result for a file the user has already moved past
// is dropped instead of flashing on screen.
class Previewer : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool visible READ visible NOTIFY visibleChanged)
    Q_PROPERTY(QVariantMap file READ file NOTIFY fileChanged)
    Q_PROPERTY(QVariantMap details READ details NOTIFY detailsChanged)
    Q_PROPERTY(bool fromFileManager READ fromFileManager NOTIFY fileChanged)
    Q_PROPERTY(QStringList prefetch READ prefetch NOTIFY prefetchChanged)
    Q_PROPERTY(int index READ index NOTIFY siblingsChanged)
    Q_PROPERTY(int count READ count NOTIFY siblingsChanged)
    Q_PROPERTY(TableModel *table READ table CONSTANT)

public:
    explicit Previewer(QObject *parent = nullptr);
    ~Previewer() override;

    static QString serviceName() { return "org.gnome.NautilusPreviewer"; }
    static QString objectPath() { return "/org/gnome/NautilusPreviewer"; }
    bool registerOnBus(QString *error);

    bool visible() const { return m_visible; }
    QVariantMap file() const { return m_file; }
    QVariantMap details() const { return m_details; }
    bool fromFileManager() const { return m_fromFileManager; }
    QStringList prefetch() const { return m_prefetch; }
    int index() const { return m_index; }
    int count() const { return int(m_siblings.size()); }
    TableModel *table() { return &m_table; }
    QString parentHandle() const { return m_parentHandle; }

    // From D-Bus: Nautilus or the gander command line.
    void showUri(const QString &uri, const QString &windowHandle, bool closeIfShown);

    Q_INVOKABLE void show(const QString &path);
    Q_INVOKABLE void close();
    // GtkDirectionType values: 2 up, 3 down, 4 left, 5 right.
    Q_INVOKABLE void navigate(int direction);
    Q_INVOKABLE void openExternally();
    Q_INVOKABLE void trash();
    Q_INVOKABLE void rename();
    Q_INVOKABLE void highlight(QQuickTextDocument *document, const QString &language, const QString &accent);
    Q_INVOKABLE void polishMarkdown(QQuickTextDocument *document);
    Q_INVOKABLE void trace(const QString &what);

signals:
    void visibleChanged();
    void fileChanged();
    void detailsChanged();
    void prefetchChanged();
    void siblingsChanged();
    void parentHandleChanged();
    void selectionEvent(uint direction);
    void renameRequested(const QString &uri);
    void trashRequested(const QString &uri);

private:
    void setVisible(bool visible);
    void merge(const QVariantMap &values);
    void startLoading();
    void loadSiblings(bool stepping);
    void updatePrefetch();
    void loadPdf(const QString &pdf);
    void startWaveform(const QString &path);
    void stopProcesses();
    template <typename Function> void runAsync(Function function);

    bool m_visible = false;
    bool m_fromFileManager = false;
    QString m_parentHandle;
    QVariantMap m_file;
    QVariantMap m_details;
    quint64 m_generation = 0;
    QStringList m_siblings;
    QString m_siblingsDirectory;
    int m_index = -1;
    QStringList m_prefetch;
    TableModel m_table;
    OfficeConverter m_office;
    QPointer<QProcess> m_waveform;
    QPointer<Highlighter> m_highlighter;
    QString m_highlighterLanguage;
    bool m_highlighting = false;
    int m_fontId = -1;
    QElapsedTimer m_clock;
    bool m_tracing = false;
};
