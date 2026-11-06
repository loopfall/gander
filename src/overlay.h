#pragma once

#include <QObject>
#include <functional>

class QQuickWindow;
namespace LayerShellQt { class Window; }

// The preview floats above everything as a wlr-layer-shell overlay: no tiling,
// no window-open animation from the compositor, no other window reflowing to
// make room, and it opens on whichever monitor has focus. Where the compositor
// has no layer shell (GNOME, X11) it falls back to a borderless full-screen
// window with the same transparent backdrop.
class Overlay : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool layerShell READ layerShell CONSTANT)

public:
    explicit Overlay(QObject *parent = nullptr);

    bool layerShell() const { return m_layer != nullptr; }
    void attach(QQuickWindow *window);

    // Maps the window once, invisibly and without taking the keyboard, so the
    // GPU context, shaders and glyph caches exist before the first Space.
    void warmUp(std::function<bool()> canHide);

    // Called right before every show.
    Q_INVOKABLE void prepareShow();

private:
    QQuickWindow *m_window = nullptr;
    LayerShellQt::Window *m_layer = nullptr;
};
