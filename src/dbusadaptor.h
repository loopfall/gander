#pragma once

#include <QDBusAbstractAdaptor>

class Previewer;

// org.gnome.NautilusPreviewer2, the interface Nautilus calls when Space is
// pressed (Sushi's). Owning that bus name is the whole integration: Nautilus
// needs no plugin and no patch.
class NautilusPreviewer2Adaptor : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.gnome.NautilusPreviewer2")
    Q_PROPERTY(bool Visible READ visible)
    Q_PROPERTY(QString ParentHandle READ parentHandle)

public:
    explicit NautilusPreviewer2Adaptor(Previewer *previewer);

    bool visible() const;
    QString parentHandle() const;
    void notifyChanged(const QString &property, const QVariant &value);

public slots:
    void ShowFile(const QString &uri, const QString &windowHandle, bool closeIfShown, const QString &activationToken);
    void Close();

signals:
    // GtkDirectionType: 2 up, 3 down, 4 left, 5 right. Nautilus moves its
    // selection and calls ShowFile with the new file.
    void SelectionEvent(uint direction);
    void NavigateTo(const QString &uri);
    void Rename(const QString &uri);
    void Trash(const QString &uri);

private:
    Previewer *m_previewer;
};

// The original interface, for older Nautilus and other file managers that
// still speak it.
class NautilusPreviewerAdaptor : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.gnome.NautilusPreviewer")

public:
    explicit NautilusPreviewerAdaptor(Previewer *previewer);

public slots:
    void ShowFile(const QString &uri, int windowId, bool closeIfShown);
    void Close();

private:
    Previewer *m_previewer;
};
