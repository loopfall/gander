import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// Pages render on demand at the exact pixel width they're shown at, a couple
// of screens ahead of the scroll. Office documents use the same view once
// LibreOffice has made the PDF; until then their text shows straight from the
// file, so there's something to read within milliseconds.
Item {
    id: view
    property var file: ({})
    property var details: ({})
    property bool active
    property real zoom: 1
    property color textColor
    property color dimColor
    readonly property Flickable flickable: pages.count > 0 ? pages : quick
    readonly property real dpr: Screen.devicePixelRatio
    readonly property bool ready: active && details.docId !== undefined
    readonly property var aspects: details.pageAspects || []

    Rectangle {
        anchors.fill: parent
        color: "#1c1c20"
    }

    ListView {
        id: pages
        anchors.fill: parent
        model: view.ready ? (view.details.pageCount || 0) : 0
        spacing: 14
        topMargin: 18
        bottomMargin: 18
        clip: true
        cacheBuffer: Math.max(0, height * 2)
        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: view.zoom > 1 ? Flickable.HorizontalAndVerticalFlick : Flickable.VerticalFlick
        contentWidth: Math.max(width, pageWidth + 36)
        reuseItems: true
        readonly property real pageWidth: Math.max(120, (width - 36) * view.zoom)

        delegate: Item {
            id: page
            required property int index
            readonly property real aspect: view.aspects[index] || 1.414
            width: pages.contentWidth
            height: Math.round(pages.pageWidth * aspect)

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: pages.pageWidth
                height: parent.height
                color: "#ffffff"

                // Nautilus's thumbnail of the first page, until it renders.
                Image {
                    anchors.fill: parent
                    visible: page.index === 0 && render.status !== Image.Ready && view.file.kind === "pdf"
                    source: visible && view.file.thumbnail ? view.file.thumbnail : ""
                    asynchronous: true
                    fillMode: Image.Stretch
                }
                Image {
                    id: render
                    anchors.fill: parent
                    asynchronous: true
                    cache: false
                    retainWhileLoading: true
                    smooth: true
                    // Widths are rounded so a resize animation doesn't ask for
                    // a new render every frame.
                    source: view.ready
                        ? "image://pdf/" + view.details.docId + "/" + page.index + "/"
                          + Math.round(pages.pageWidth * view.dpr / 48) * 48
                        : ""
                }
            }
        }

        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
        ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }
    }

    Rectangle {
        anchors { right: parent.right; bottom: parent.bottom; margins: 14 }
        visible: pages.count > 1
        opacity: pages.moving ? 1 : 0.0
        Behavior on opacity { NumberAnimation { duration: pages.moving ? 80 : 600 } }
        width: pageLabel.implicitWidth + 20
        height: 26
        radius: 13
        color: "#dd111114"
        Label {
            id: pageLabel
            anchors.centerIn: parent
            text: qsTr("%1 of %2").arg(Math.min(pages.count, pages.indexAt(10, pages.contentY + pages.height / 3) + 1 || 1)).arg(pages.count)
            color: "#ececec"
            font.pixelSize: 12
        }
    }

    // Before the PDF is ready: the document's words.
    Flickable {
        id: quick
        anchors.fill: parent
        visible: !view.ready && view.file.kind === "office"
        contentWidth: width
        contentHeight: quickText.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        Rectangle {
            anchors.fill: parent
            color: "#1c1c20"
        }
        Text {
            id: quickText
            width: parent.width
            padding: 36
            topPadding: 56
            text: view.details.quickText || ""
            color: view.textColor
            font.pixelSize: 14
            lineHeight: 1.35
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
        }
        ScrollBar.vertical: ScrollBar {}
    }

    Rectangle {
        anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 14 }
        visible: view.active && view.details.converting === true
        width: busyRow.implicitWidth + 24
        height: 30
        radius: 15
        color: "#ee111114"
        border.color: "#26ffffff"
        Row {
            id: busyRow
            anchors.centerIn: parent
            spacing: 8
            BusyIndicator {
                anchors.verticalCenter: parent.verticalCenter
                width: 18
                height: 18
                running: parent.parent.visible
            }
            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Laying out the document…")
                color: "#ececec"
                font.pixelSize: 12
            }
        }
    }
}
