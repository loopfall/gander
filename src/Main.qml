import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Effects
import QtQuick.Window
import QtMultimedia

import "Format.js" as Format

// One transparent surface over the whole monitor: a dimmed backdrop (click it
// to close) and the card in the middle. Every viewer exists from the start and
// only swaps what it shows, so changing files never builds new QML objects.
Window {
    id: win
    visible: false
    color: "transparent"
    title: "Gander"
    flags: overlay.layerShell ? Qt.Window : (Qt.Window | Qt.FramelessWindowHint)
    width: 1280
    height: 800

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius

    readonly property var file: previewer.file
    readonly property var details: previewer.details
    readonly property string kind: file.kind || ""
    readonly property real dpr: Screen.devicePixelRatio
    // One decode size for every image, the monitor's, so a neighbour decoded
    // ahead of time is the exact pixmap the viewer asks for when it arrives.
    readonly property size decodeSize: Qt.size(Math.round(Screen.width * dpr), Math.round(Screen.height * dpr))

    readonly property color cardColor: "#131316"
    readonly property color textColor: "#ececec"
    readonly property color dimColor: "#9b9ba3"

    property bool shown: false
    property bool settled: false
    property bool expanded: false
    property bool helpVisible: false
    property real zoom: 1

    readonly property bool showsGeneric: kind === "generic" || kind === "missing"
        || ((file.error || details.error) !== undefined && !(kind === "office" && details.quickText)
            && kind !== "image" && kind !== "animated" && kind !== "video" && kind !== "audio")

    Connections {
        target: previewer
        function onVisibleChanged() {
            if (previewer.visible)
                win.present()
            else
                win.dismiss()
        }
        function onFileChanged() {
            win.awaitingFrame = true
            win.zoom = 1
            win.syncMedia()
            win.syncPool()
        }
        function onPrefetchChanged() {
            win.syncPool()
        }
    }

    // GANDER_TRACE: when the first frame of a preview reaches the screen.
    property bool awaitingFrame: false
    onFrameSwapped: {
        if (awaitingFrame) {
            awaitingFrame = false
            previewer.trace("first frame")
        }
    }

    function present() {
        awaitingFrame = true
        closeAnimation.stop()
        if (!shown) {
            shown = true
            settled = false
            stage.opacity = 0
            stage.scale = 0.965
            backdrop.opacity = 0
            overlay.prepareShow()
            if (overlay.layerShell)
                win.visible = true
            else
                win.showFullScreen()
        }
        win.requestActivate()
        openAnimation.restart()
    }

    function dismiss() {
        openAnimation.stop()
        helpVisible = false
        player.pause()
        closeAnimation.restart()
    }

    function finishDismiss() {
        if (previewer.visible)
            return
        shown = false
        win.visible = false
        player.stop()
        player.source = ""
        clearPool()
    }

    function syncMedia() {
        if (kind === "video" || kind === "audio") {
            if (player.source.toString() !== file.url) {
                player.source = file.url
                player.play()
            }
        } else if (player.source.toString() !== "") {
            player.stop()
            player.source = ""
        }
    }

    // Decoded neighbours, kept by URL. An entry stays while it is the current
    // file or a likely next one, so stepping back and forth never decodes the
    // same photo twice.
    property var pool: ({})
    function poolWanted() {
        const wanted = previewer.prefetch.slice()
        if (kind === "image")
            wanted.push(file.url)
        return wanted
    }
    function syncPool() {
        for (const url of poolWanted())
            if (!pool[url])
                pool[url] = prefetchComponent.createObject(prefetchHolder, { source: url })
        Qt.callLater(prunePool)
    }
    function prunePool() {
        const wanted = poolWanted()
        for (const url in pool) {
            if (wanted.indexOf(url) < 0) {
                pool[url].destroy()
                delete pool[url]
            }
        }
    }
    function clearPool() {
        for (const url in pool)
            pool[url].destroy()
        pool = {}
    }

    function subtitle() {
        const parts = []
        const d = details
        switch (kind) {
        case "image":
        case "animated":
            if (file.width > 0)
                parts.push(file.width + " × " + file.height)
            break
        case "video":
            if (videoView.resolution.width > 0)
                parts.push(videoView.resolution.width + " × " + videoView.resolution.height)
            if (player.duration > 0)
                parts.push(Format.time(player.duration))
            break
        case "audio":
            if (file.suffix)
                parts.push(file.suffix.toUpperCase())
            if (d.bitrate > 0)
                parts.push(qsTr("%1 kbps").arg(d.bitrate))
            if (d.durationMs > 0)
                parts.push(Format.time(d.durationMs))
            break
        case "pdf":
        case "office":
            if (d.pageCount > 0)
                parts.push(qsTr("%n pages", "", d.pageCount))
            break
        case "text":
        case "markdown":
            if (d.lines !== undefined)
                parts.push(qsTr("%n lines", "", d.lines))
            break
        case "table":
            if (d.rows !== undefined)
                parts.push(qsTr("%n rows", "", d.rows))
            break
        case "folder":
            if (d.count !== undefined)
                parts.push(qsTr("%n items", "", d.count))
            if (d.totalText)
                parts.push((d.partial ? "> " : "") + d.totalText)
            return parts.join("  ·  ")
        case "archive":
            if (d.files !== undefined)
                parts.push(qsTr("%n files", "", d.files))
            if (d.totalText)
                parts.push(qsTr("%1 unpacked").arg(d.totalText))
            break
        }
        if (parts.length === 0 && file.mimeComment)
            parts.push(file.mimeComment)
        if (file.sizeText)
            parts.push(file.sizeText)
        return parts.join("  ·  ")
    }

    // The card's body size for what is shown: images and video keep their
    // shape, documents take the height, small things stay small.
    readonly property int headerHeight: 52
    readonly property real maxWidth: expanded ? width : Math.max(320, width - 112)
    readonly property real maxBodyHeight: (expanded ? height : height - 112) - headerHeight
    function bodySize() {
        const w = maxWidth, h = maxBodyHeight
        switch (kind) {
        case "image":
        case "animated": {
            const iw = (file.width || 0) / dpr, ih = (file.height || 0) / dpr
            if (!(iw > 0 && ih > 0))
                return Qt.size(Math.min(w, 960), Math.min(h, 640))
            const s = Math.min(w / iw, h / ih, expanded ? 1e6 : 1)
            return Qt.size(Math.max(Math.round(iw * s), 360), Math.max(Math.round(ih * s), 220))
        }
        case "video": {
            const a = videoView.aspect
            const vw = Math.min(w, h * a, expanded ? 1e6 : Math.max(960, w * 0.72))
            return Qt.size(Math.max(Math.round(vw), 480), Math.max(Math.round(vw / a), 270))
        }
        case "audio":
            return Qt.size(Math.min(w, 800), Math.min(h, 320))
        case "pdf":
        case "office": {
            const aspects = details.pageAspects
            const slides = ["ppt", "pptx", "pptm", "odp", "key"].indexOf(file.suffix) >= 0
            const a = aspects && aspects.length ? aspects[0] : (slides ? 0.5625 : 1.414)
            return Qt.size(Math.min(w, Math.max(680, (h - 40) / a + 56)), h)
        }
        case "text": {
            const lines = details.lines !== undefined ? details.lines : (file.size || 0) / 40
            return Qt.size(Math.min(w, 1000), Math.min(h, Math.max(240, lines * 19 + 44)))
        }
        case "markdown": {
            const lines = details.lines !== undefined ? details.lines : (file.size || 0) / 50
            return Qt.size(Math.min(w, 880), Math.min(h, Math.max(260, lines * 30 + 80)))
        }
        case "table":
            return Qt.size(Math.min(w, 1160), Math.min(h, Math.max(240, (details.rows || 20) * 30 + 16)))
        case "folder":
        case "archive":
            return Qt.size(Math.min(w, 720), Math.min(h, Math.max(260, ((details.entries || []).length) * 34 + 120)))
        case "font":
            return Qt.size(Math.min(w, 920), Math.min(h, 600))
        }
        return Qt.size(Math.min(w, 560), Math.min(h, 320))
    }

    MediaPlayer {
        id: player
        audioOutput: AudioOutput { id: audioOut }
        videoOutput: videoView.output
        loops: kind === "video" && player.duration > 0 && player.duration < 20000 ? MediaPlayer.Infinite : 1
    }

    Item {
        id: prefetchHolder
        visible: false
    }
    Component {
        id: prefetchComponent
        // Must match ImagePreview's still image option for option (fill mode
        // and orientation are part of the cache key), or it decodes twice.
        Image {
            asynchronous: true
            autoTransform: true
            cache: true
            fillMode: Image.PreserveAspectFit
            sourceSize: win.decodeSize
        }
    }

    Item {
        id: root
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        Item {
            id: backdrop
            anchors.fill: parent
            Rectangle {
                anchors.fill: parent
                color: "#000000"
                opacity: win.expanded ? 0.92 : 0.42
                Behavior on opacity { NumberAnimation { duration: 160 } }
            }
            MouseArea {
                anchors.fill: parent
                onClicked: previewer.close()
            }
        }

        Item {
            id: stage
            anchors.centerIn: parent
            width: card.width
            height: card.height

            RectangularShadow {
                anchors.fill: card
                radius: card.radius
                blur: 56
                offset.y: 14
                color: "#a0000000"
                visible: !win.expanded
            }

            Rectangle {
                id: card
                readonly property size target: win.bodySize()
                width: target.width
                height: target.height + win.headerHeight
                radius: win.expanded ? 0 : theme.radius
                color: win.cardColor
                border.color: "#26ffffff"
                border.width: win.expanded ? 0 : 1
                clip: true

                Behavior on width { enabled: win.settled; NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                Behavior on height { enabled: win.settled; NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

                // Clicks on the card must not reach the backdrop.
                MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons }

                HeaderBar {
                    id: header
                    anchors { left: parent.left; right: parent.right; top: parent.top }
                    height: win.headerHeight
                    file: win.file
                    details: win.details
                    subtitle: win.subtitle()
                    expanded: win.expanded
                    textColor: win.textColor
                    dimColor: win.dimColor
                    position: !previewer.fromFileManager && previewer.count > 1 && previewer.index >= 0
                              ? (previewer.index + 1) + " / " + previewer.count : ""
                    onOpenRequested: previewer.openExternally()
                    onExpandRequested: win.expanded = !win.expanded
                    onCloseRequested: previewer.close()
                }

                Item {
                    id: body
                    anchors { left: parent.left; right: parent.right; top: header.bottom; bottom: parent.bottom }
                    anchors.margins: 1
                    anchors.topMargin: 0

                    ImagePreview {
                        anchors.fill: parent
                        active: (win.kind === "image" || win.kind === "animated") && win.shown
                        visible: active
                        file: win.file
                        decodeSize: win.decodeSize
                        zoom: win.zoom
                        expanded: win.expanded
                        onZoomRequested: (value) => win.zoom = value
                    }

                    VideoPreview {
                        id: videoView
                        anchors.fill: parent
                        active: win.kind === "video" && win.shown
                        visible: active
                        file: win.file
                        player: player
                        audio: audioOut
                    }

                    AudioPreview {
                        anchors.fill: parent
                        active: win.kind === "audio" && win.shown
                        visible: active
                        file: win.file
                        details: win.details
                        player: player
                        textColor: win.textColor
                        dimColor: win.dimColor
                        cardColor: win.cardColor
                    }

                    PdfPreview {
                        id: pdfView
                        anchors.fill: parent
                        active: (win.kind === "pdf" || win.kind === "office") && win.shown && !win.showsGeneric
                        visible: active
                        file: win.file
                        details: win.details
                        zoom: win.zoom
                        textColor: win.textColor
                        dimColor: win.dimColor
                    }

                    TextPreview {
                        id: textView
                        anchors.fill: parent
                        active: (win.kind === "text" || win.kind === "markdown") && win.shown && !win.showsGeneric
                        visible: active
                        markdown: win.kind === "markdown"
                        file: win.file
                        details: win.details
                        textColor: win.textColor
                        dimColor: win.dimColor
                    }

                    TablePreview {
                        id: tableView
                        anchors.fill: parent
                        active: win.kind === "table" && win.shown && !win.showsGeneric
                        visible: active
                        textColor: win.textColor
                    }

                    ListPreview {
                        id: listView
                        anchors.fill: parent
                        active: (win.kind === "folder" || win.kind === "archive") && win.shown && !win.showsGeneric
                        visible: active
                        file: win.file
                        details: win.details
                        textColor: win.textColor
                        dimColor: win.dimColor
                    }

                    FontPreview {
                        id: fontView
                        anchors.fill: parent
                        active: win.kind === "font" && win.shown && !win.showsGeneric
                        visible: active
                        details: win.details
                        textColor: win.textColor
                        dimColor: win.dimColor
                    }

                    GenericPreview {
                        anchors.fill: parent
                        visible: win.showsGeneric && win.shown
                        file: win.file
                        details: win.details
                        textColor: win.textColor
                        dimColor: win.dimColor
                    }
                }

                HelpOverlay {
                    anchors.fill: parent
                    visible: win.helpVisible
                    fromFileManager: previewer.fromFileManager
                    textColor: win.textColor
                    dimColor: win.dimColor
                    onDismissed: win.helpVisible = false
                }
            }
        }
    }

    ParallelAnimation {
        id: openAnimation
        NumberAnimation { target: stage; property: "opacity"; to: 1; duration: 110; easing.type: Easing.OutCubic }
        NumberAnimation { target: stage; property: "scale"; to: 1; duration: 170; easing.type: Easing.OutCubic }
        NumberAnimation { target: backdrop; property: "opacity"; to: 1; duration: 140; easing.type: Easing.OutCubic }
        onFinished: win.settled = true
    }

    ParallelAnimation {
        id: closeAnimation
        NumberAnimation { target: stage; property: "opacity"; to: 0; duration: 90; easing.type: Easing.InCubic }
        NumberAnimation { target: stage; property: "scale"; to: 0.975; duration: 90; easing.type: Easing.InCubic }
        NumberAnimation { target: backdrop; property: "opacity"; to: 0; duration: 100; easing.type: Easing.InCubic }
        onFinished: win.finishDismiss()
    }

    // Keys. Arrows step through files (in Nautilus they move its selection);
    // Space, Escape and Q close, like Quick Look.
    Shortcut { sequences: ["Escape", "Space", "Q"]; context: Qt.ApplicationShortcut; onActivated: win.helpVisible ? (win.helpVisible = false) : previewer.close() }
    Shortcut { sequence: "Left"; context: Qt.ApplicationShortcut; onActivated: previewer.navigate(4) }
    Shortcut { sequence: "Right"; context: Qt.ApplicationShortcut; onActivated: previewer.navigate(5) }
    Shortcut { sequence: "Up"; context: Qt.ApplicationShortcut; onActivated: previewer.navigate(2) }
    Shortcut { sequence: "Down"; context: Qt.ApplicationShortcut; onActivated: previewer.navigate(3) }
    Shortcut { sequences: ["Return", "Enter"]; context: Qt.ApplicationShortcut; onActivated: previewer.openExternally() }
    Shortcut { sequence: "F"; context: Qt.ApplicationShortcut; onActivated: win.expanded = !win.expanded }
    Shortcut { sequences: ["?", "F1"]; context: Qt.ApplicationShortcut; onActivated: win.helpVisible = !win.helpVisible }
    Shortcut { sequences: ["+", "="]; context: Qt.ApplicationShortcut; onActivated: win.zoom = Math.min(8, win.zoom * 1.25) }
    Shortcut { sequence: "-"; context: Qt.ApplicationShortcut; onActivated: win.zoom = Math.max(1, win.zoom / 1.25) }
    Shortcut { sequence: "0"; context: Qt.ApplicationShortcut; onActivated: win.zoom = 1 }
    Shortcut { sequence: "K"; context: Qt.ApplicationShortcut; enabled: player.source.toString() !== ""
               onActivated: player.playbackState === MediaPlayer.PlayingState ? player.pause() : player.play() }
    Shortcut { sequence: "J"; context: Qt.ApplicationShortcut; enabled: player.seekable; onActivated: player.position = Math.max(0, player.position - 5000) }
    Shortcut { sequence: "L"; context: Qt.ApplicationShortcut; enabled: player.seekable; onActivated: player.position = Math.min(player.duration, player.position + 5000) }
    Shortcut { sequence: "M"; context: Qt.ApplicationShortcut; onActivated: audioOut.muted = !audioOut.muted }
    Shortcut { sequence: "PgDown"; context: Qt.ApplicationShortcut; onActivated: win.scrollPage(1) }
    Shortcut { sequence: "PgUp"; context: Qt.ApplicationShortcut; onActivated: win.scrollPage(-1) }
    Shortcut { sequence: "Home"; context: Qt.ApplicationShortcut; onActivated: win.scrollPage(-1e6) }
    Shortcut { sequence: "End"; context: Qt.ApplicationShortcut; onActivated: win.scrollPage(1e6) }
    Shortcut { sequence: "Delete"; context: Qt.ApplicationShortcut; enabled: previewer.fromFileManager; onActivated: previewer.trash() }
    Shortcut { sequence: "F2"; context: Qt.ApplicationShortcut; enabled: previewer.fromFileManager; onActivated: previewer.rename() }

    function scrollPage(pages) {
        const target = pdfView.active ? pdfView.flickable : textView.active ? textView.flickable
                     : tableView.active ? tableView.flickable : listView.active ? listView.flickable
                     : fontView.active ? fontView.flickable : null
        if (!target)
            return
        const limit = Math.max(0, target.contentHeight - target.height)
        target.contentY = Math.max(0, Math.min(limit, target.contentY + pages * target.height * 0.9))
    }
}
