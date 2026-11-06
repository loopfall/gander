import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

import "Format.js" as Format

// Anything without a dedicated view: its thumbnail if Nautilus made one,
// otherwise its icon, and the facts. Errors show here too, in words.
Item {
    id: view
    property var file: ({})
    property var details: ({})
    property color textColor
    property color dimColor
    readonly property string error: file.error || details.error || ""

    Row {
        anchors.centerIn: parent
        spacing: 28
        width: Math.min(parent.width - 56, implicitWidth)

        Image {
            id: art
            anchors.verticalCenter: parent.verticalCenter
            width: 128
            height: 128
            fillMode: Image.PreserveAspectFit
            sourceSize: Qt.size(256, 256)
            asynchronous: true
            source: view.visible ? (view.file.thumbnail || Format.icon(view.file)) : ""
        }

        Column {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 5
            width: Math.min(360, view.width - art.width - 100)

            Label {
                width: parent.width
                text: view.file.name || ""
                color: view.textColor
                font.pixelSize: 18
                font.weight: Font.DemiBold
                wrapMode: Text.WrapAnywhere
                maximumLineCount: 3
                elide: Text.ElideRight
            }
            Label {
                width: parent.width
                visible: text !== ""
                text: view.file.mimeComment || ""
                color: view.dimColor
                font.pixelSize: 13
            }
            Label {
                width: parent.width
                visible: text !== ""
                text: [view.file.sizeText || "", view.file.modified || ""].filter(s => s !== "").join("  ·  ")
                color: view.dimColor
                font.pixelSize: 13
            }
            Label {
                width: parent.width
                visible: view.error !== ""
                topPadding: 8
                text: view.error
                color: "#ff9f7a"
                font.pixelSize: 13
                wrapMode: Text.Wrap
            }
        }
    }
}
