import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// Every key, on ?.
Rectangle {
    id: help
    property bool fromFileManager
    property color textColor
    property color dimColor
    signal dismissed()
    color: "#f2111114"

    MouseArea {
        anchors.fill: parent
        onClicked: help.dismissed()
    }

    readonly property var keys: [
        ["Space  Esc  Q", qsTr("Close")],
        ["← → ↑ ↓", qsTr("Previous or next file")],
        ["Enter", qsTr("Open in the default app")],
        ["F", qsTr("Fill the screen")],
        ["+  −  0", qsTr("Zoom in, out, reset")],
        ["K  J  L", qsTr("Play or pause, back, forward")],
        ["M", qsTr("Mute")],
        ["PgUp  PgDn", qsTr("Scroll a page")],
    ].concat(fromFileManager ? [["Delete", qsTr("Move to trash")], ["F2", qsTr("Rename")]] : [])
     .concat([["?", qsTr("Show these keys")]])

    Flickable {
        anchors.fill: parent
        contentHeight: grid.implicitHeight + 48
        clip: true
        Grid {
            id: grid
            anchors.horizontalCenter: parent.horizontalCenter
            y: Math.max(24, (help.height - implicitHeight) / 2)
            columns: 2
            columnSpacing: 28
            rowSpacing: 12
            Repeater {
                model: help.keys.length * 2
                Label {
                    required property int index
                    readonly property var entry: help.keys[Math.floor(index / 2)]
                    text: index % 2 === 0 ? entry[0] : entry[1]
                    color: index % 2 === 0 ? theme.accent : help.textColor
                    font.pixelSize: 14
                    font.family: index % 2 === 0 ? "monospace" : Qt.application.font.family
                    horizontalAlignment: index % 2 === 0 ? Text.AlignRight : Text.AlignLeft
                    width: index % 2 === 0 ? 140 : implicitWidth
                }
            }
        }
    }
}
