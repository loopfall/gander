#pragma once

#include <QFileInfo>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QTemporaryDir>

#include <functional>
#include <memory>

// Office documents become PDFs through LibreOffice, once: the result is cached
// by path, size and modification time, so the second look is as fast as a
// PDF. LibreOffice runs with its own profile, so a converter running here
// never collides with a LibreOffice window the user has open (which would
// otherwise swallow the request and convert nothing).
class OfficeConverter : public QObject {
    Q_OBJECT

public:
    using Done = std::function<void(const QString &pdf, const QString &error)>;

    explicit OfficeConverter(const QString &cacheDirectory = {}, QObject *parent = nullptr);
    ~OfficeConverter() override;

    QString cacheDirectory() const { return m_cacheDirectory; }
    QString cachedPdf(const QString &path) const;
    static QString cacheKey(const QFileInfo &info);
    static QString executable();

    // Only one conversion runs at a time; starting another stops the first.
    void convert(const QString &path, Done done);
    void cancel();

    // Creates the LibreOffice profile in the background, once per machine, so
    // the first document anybody previews doesn't pay for it.
    void prepare();

    // Removes cached PDFs nobody has looked at for a month.
    void prune();

    // The words of an OOXML or OpenDocument file, straight from the XML inside
    // the zip, to show while LibreOffice lays out the real thing. Runs in a
    // worker thread; empty for formats it can't read this way.
    static QString quickText(const QString &path);

private:
    QString m_cacheDirectory;
    QString profileUrl() const;
    QPointer<QProcess> m_process;
    std::unique_ptr<QTemporaryDir> m_outputDirectory;
    Done m_done;
};
