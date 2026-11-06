import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// A font file set in itself, from display size down to text size.
Item {
    id: view
    property var details: ({})
    property bool active
    property color textColor
    property color dimColor
    readonly property Flickable flickable: flick
    readonly property string family: details.fontFamily || ""

    Flickable {
        id: flick
        anchors.fill: parent
        contentHeight: column.implicitHeight + 64
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: column
            x: 36
            y: 32
            width: flick.width - 72
            spacing: 14
            visible: view.family !== ""

            Label {
                width: parent.width
                text: view.family
                color: view.textColor
                font.family: view.family
                font.pixelSize: 44
                elide: Text.ElideRight
            }
            Repeater {
                model: [
                    { size: 72, text: "Aa Bb Cc 0123" },
                    { size: 34, text: qsTr("The quick brown fox jumps over the lazy dog") },
                    { size: 22, text: "ABCDEFGHIJKLMNOPQRSTUVWXYZ" },
                    { size: 22, text: "abcdefghijklmnopqrstuvwxyz" },
                    { size: 22, text: "0123456789 !?&@#%*()[]{}<>/\\ áčďéíľňóôŕšťúýž ÄÖÜß" },
                    { size: 15, text: qsTr("The quick brown fox jumps over the lazy dog") }
                ]
                Label {
                    required property var modelData
                    width: column.width
                    text: modelData.text
                    color: view.textColor
                    font.family: view.family
                    font.pixelSize: modelData.size
                    wrapMode: Text.Wrap
                }
            }
        }
        ScrollBar.vertical: ScrollBar {}
    }
}
