#include "overlay.h"

#include <QGuiApplication>
#include <QQuickWindow>

#include <LayerShellQt/Window>

Overlay::Overlay(QObject *parent) : QObject(parent) {}

static bool wantsLayerShell() {
    if (!QGuiApplication::platformName().startsWith("wayland"))
        return false;
    if (!qEnvironmentVariableIsEmpty("GANDER_NO_LAYER_SHELL"))
        return false;
    // Mutter is the one major Wayland compositor without wlr-layer-shell.
    return !qEnvironmentVariable("XDG_CURRENT_DESKTOP").contains("GNOME", Qt::CaseInsensitive);
}

void Overlay::attach(QQuickWindow *window) {
    m_window = window;
    // Keep the GPU context and scene graph across hide and show, so showing
    // again only maps a surface instead of rebuilding the renderer.
    window->setPersistentGraphics(true);
    window->setPersistentSceneGraph(true);
    if (!wantsLayerShell())
        return;

    m_layer = LayerShellQt::Window::get(window);
    m_layer->setScope("gander");
    m_layer->setLayer(LayerShellQt::Window::LayerOverlay);
    m_layer->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorTop | LayerShellQt::Window::AnchorBottom
                                                      | LayerShellQt::Window::AnchorLeft | LayerShellQt::Window::AnchorRight));
    // -1: cover panels and bars too, and don't push anything aside.
    m_layer->setExclusiveZone(-1);
    m_layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityExclusive);
    m_layer->setWantsToBeOnActiveScreen(true);
    m_layer->setActivateOnShow(true);
}

void Overlay::prepareShow() {
    if (!m_layer)
        return;
    m_layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityExclusive);
    // Each show is a new surface; ask again so it lands on the monitor the
    // user is looking at now, not the one it opened on last time.
    m_layer->setWantsToBeOnActiveScreen(true);
}

void Overlay::warmUp(std::function<bool()> canHide) {
    if (!m_layer || !m_window)
        return;
    m_layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
    auto connection = std::make_shared<QMetaObject::Connection>();
    *connection = connect(m_window, &QQuickWindow::frameSwapped, this, [this, connection, canHide] {
        disconnect(*connection);
        // A Space that arrived during the warm-up owns the window now.
        if (canHide())
            m_window->hide();
        m_layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityExclusive);
    });
    m_window->show();
}
