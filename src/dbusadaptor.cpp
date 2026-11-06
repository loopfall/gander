#include "dbusadaptor.h"

#include "previewer.h"

#include <QDBusConnection>
#include <QDBusMessage>

NautilusPreviewer2Adaptor::NautilusPreviewer2Adaptor(Previewer *previewer)
    : QDBusAbstractAdaptor(previewer), m_previewer(previewer) {
    setAutoRelaySignals(false);
    connect(previewer, &Previewer::selectionEvent, this, &NautilusPreviewer2Adaptor::SelectionEvent);
    connect(previewer, &Previewer::renameRequested, this, &NautilusPreviewer2Adaptor::Rename);
    connect(previewer, &Previewer::trashRequested, this, &NautilusPreviewer2Adaptor::Trash);
    connect(previewer, &Previewer::visibleChanged, this, [this] { notifyChanged("Visible", m_previewer->visible()); });
    connect(previewer, &Previewer::parentHandleChanged, this,
            [this] { notifyChanged("ParentHandle", m_previewer->parentHandle()); });
}

bool NautilusPreviewer2Adaptor::visible() const {
    return m_previewer->visible();
}

QString NautilusPreviewer2Adaptor::parentHandle() const {
    return m_previewer->parentHandle();
}

void NautilusPreviewer2Adaptor::notifyChanged(const QString &property, const QVariant &value) {
    // QtDBus doesn't emit PropertiesChanged by itself; Nautilus watches
    // Visible to know whether a new selection should update the preview.
    QDBusMessage signal = QDBusMessage::createSignal(Previewer::objectPath(), "org.freedesktop.DBus.Properties",
                                                     "PropertiesChanged");
    signal << QString("org.gnome.NautilusPreviewer2") << QVariantMap{{property, value}} << QStringList();
    QDBusConnection::sessionBus().send(signal);
}

void NautilusPreviewer2Adaptor::ShowFile(const QString &uri, const QString &windowHandle, bool closeIfShown,
                                         const QString &) {
    m_previewer->showUri(uri, windowHandle, closeIfShown);
}

void NautilusPreviewer2Adaptor::Close() {
    m_previewer->close();
}

NautilusPreviewerAdaptor::NautilusPreviewerAdaptor(Previewer *previewer)
    : QDBusAbstractAdaptor(previewer), m_previewer(previewer) {}

void NautilusPreviewerAdaptor::ShowFile(const QString &uri, int, bool closeIfShown) {
    m_previewer->showUri(uri, "legacy", closeIfShown);
}

void NautilusPreviewerAdaptor::Close() {
    m_previewer->close();
}
