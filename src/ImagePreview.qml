import QtQuick

// Nautilus's own thumbnail paints the first frame; the real image, decoded in
// a worker thread at the monitor's size (JPEGs decode at reduced scale, which
// is several times faster), replaces it when ready. Zooming past the fit
// loads the full resolution.
Item {
    id: view
    property var file: ({})
    property bool active
    property bool expanded
    property size decodeSize
    property real zoom: 1
    signal zoomRequested(real value)
    clip: true

    readonly property bool animated: file.kind === "animated"
    readonly property real dpr: Screen.devicePixelRatio
    readonly property real naturalWidth: (file.width || 0) / dpr
    readonly property real naturalHeight: (file.height || 0) / dpr
    readonly property real fit: naturalWidth > 0
        ? Math.min(width / naturalWidth, height / naturalHeight, expanded ? 1e6 : 1) : 1
    readonly property real shownWidth: (naturalWidth > 0 ? naturalWidth * fit : width) * zoom
    readonly property real shownHeight: (naturalHeight > 0 ? naturalHeight * fit : height) * zoom

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: Math.max(width, view.shownWidth)
        contentHeight: Math.max(height, view.shownHeight)
        interactive: view.zoom > 1
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        Item {
            width: flick.contentWidth
            height: flick.contentHeight

            Image {
                id: thumbnail
                anchors.centerIn: parent
                width: view.shownWidth
                height: view.shownHeight
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                smooth: true
                source: view.active && view.file.thumbnail ? view.file.thumbnail : ""
                visible: (view.animated ? animation.status : still.status) !== Image.Ready
            }

            Image {
                id: still
                anchors.centerIn: parent
                width: view.shownWidth
                height: view.shownHeight
                visible: !view.animated && !(fullResolution.status === Image.Ready && fullResolution.visible)
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                autoTransform: true
                cache: true
                smooth: true
                mipmap: true
                sourceSize: view.decodeSize
                source: view.active && !view.animated ? view.file.url : ""
                onStatusChanged: if (status === Image.Ready) previewer.trace("image decoded")
            }

            Image {
                id: fullResolution
                anchors.centerIn: parent
                width: view.shownWidth
                height: view.shownHeight
                readonly property bool wanted: view.active && !view.animated && view.zoom > 1.01
                    && (view.file.width > view.decodeSize.width || view.file.height > view.decodeSize.height)
                visible: wanted
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                autoTransform: true
                cache: false
                smooth: true
                source: wanted ? view.file.url : ""
            }

            AnimatedImage {
                id: animation
                anchors.centerIn: parent
                width: view.shownWidth
                height: view.shownHeight
                visible: view.animated
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                cache: false
                smooth: true
                playing: view.active
                source: view.active && view.animated ? view.file.url : ""
            }
        }
    }

    WheelHandler {
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        onWheel: (event) => {
            const factor = event.angleDelta.y > 0 ? 1.15 : 1 / 1.15
            view.zoomRequested(Math.max(1, Math.min(8, view.zoom * factor)))
        }
    }

    PinchHandler {
        target: null
        onScaleChanged: (delta) => view.zoomRequested(Math.max(1, Math.min(8, view.zoom * delta)))
    }

    TapHandler {
        onDoubleTapped: view.zoomRequested(view.zoom > 1 ? 1 : Math.max(2, 1 / view.fit))
    }
}
