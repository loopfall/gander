import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// A folder's contents, or what's packed inside an archive.
Item {
    id: view
    property var file: ({})
    property var details: ({})
    property bool active
    property color textColor
    property color dimColor
    readonly property Flickable flickable: list
    readonly property var entries: active ? (details.entries || []) : []
    readonly property bool archive: file.kind === "archive"

    ListView {
        id: list
        anchors.fill: parent
        topMargin: 8
        bottomMargin: 8
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        model: view.entries
        reuseItems: true

        delegate: Item {
            required property var modelData
            width: list.width
            height: 34

            Image {
                id: entryIcon
                anchors { left: parent.left; leftMargin: 18 + (view.archive ? Math.min(6, modelData.depth || 0) * 16 : 0); verticalCenter: parent.verticalCenter }
                width: 20
                height: 20
                sourceSize: Qt.size(40, 40)
                source: "image://gander/icon/" + modelData.icon
            }
            Label {
                anchors { left: entryIcon.right; leftMargin: 10; right: sizeLabel.left; rightMargin: 12; verticalCenter: parent.verticalCenter }
                text: modelData.name
                color: view.textColor
                font.pixelSize: 13
                elide: Text.ElideMiddle
            }
            Label {
                id: sizeLabel
                anchors { right: parent.right; rightMargin: 18; verticalCenter: parent.verticalCenter }
                text: modelData.size || ""
                color: view.dimColor
                font.pixelSize: 12
            }
        }

        footer: Label {
            width: list.width
            visible: (view.details.count || 0) > view.entries.length
            height: visible ? 36 : 0
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            text: qsTr("and %n more", "", (view.details.count || 0) - view.entries.length)
            color: view.dimColor
            font.pixelSize: 12
        }

        ScrollBar.vertical: ScrollBar {}
    }

    Label {
        anchors.centerIn: parent
        visible: view.active && view.details.count === 0
        text: view.archive ? qsTr("Empty archive") : qsTr("Empty folder")
        color: view.dimColor
        font.pixelSize: 14
    }
}
