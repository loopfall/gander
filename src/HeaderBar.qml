import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

import "Format.js" as Format

Item {
    id: bar
    property var file: ({})
    property var details: ({})
    property string subtitle
    property string position
    property bool expanded
    property color textColor
    property color dimColor
    signal openRequested()
    signal expandRequested()
    signal closeRequested()

    Image {
        id: icon
        anchors { left: parent.left; leftMargin: 16; verticalCenter: parent.verticalCenter }
        width: 22
        height: 22
        sourceSize: Qt.size(44, 44)
        source: Format.icon(bar.file)
        asynchronous: false
    }

    Column {
        anchors {
            left: icon.right; leftMargin: 11
            right: buttons.left; rightMargin: 12
            verticalCenter: parent.verticalCenter
        }
        spacing: 1
        Label {
            width: parent.width
            text: bar.file.name || ""
            color: bar.textColor
            font.pixelSize: 14
            font.weight: Font.DemiBold
            elide: Text.ElideMiddle
        }
        Label {
            width: parent.width
            text: bar.subtitle
            color: bar.dimColor
            font.pixelSize: 12
            elide: Text.ElideRight
            visible: text !== ""
        }
    }

    Row {
        id: buttons
        anchors { right: parent.right; rightMargin: 10; verticalCenter: parent.verticalCenter }
        spacing: 4

        Label {
            anchors.verticalCenter: parent.verticalCenter
            visible: bar.position !== ""
            text: bar.position
            color: bar.dimColor
            font.pixelSize: 12
            rightPadding: 8
        }
        HeaderButton {
            anchors.verticalCenter: parent.verticalCenter
            visible: bar.file.kind !== "missing" && bar.file.kind !== undefined
            iconSource: "qrc:/icons/open.svg"
            text: bar.details.openWith ? qsTr("Open with %1").arg(bar.details.openWith) : qsTr("Open")
            tip: qsTr("Open in the default app (Enter)")
            textColor: bar.textColor
            onClicked: bar.openRequested()
        }
        HeaderButton {
            anchors.verticalCenter: parent.verticalCenter
            iconSource: bar.expanded ? "qrc:/icons/collapse.svg" : "qrc:/icons/expand.svg"
            tip: bar.expanded ? qsTr("Smaller (F)") : qsTr("Fill the screen (F)")
            onClicked: bar.expandRequested()
        }
        HeaderButton {
            anchors.verticalCenter: parent.verticalCenter
            iconSource: "qrc:/icons/close.svg"
            tip: qsTr("Close (Space)")
            onClicked: bar.closeRequested()
        }
    }

    Rectangle {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: 1
        color: "#1affffff"
    }
}
