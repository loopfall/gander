import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// A flat button for the header: an icon, optionally a label, a hover wash and
// a tooltip naming its key.
Rectangle {
    id: button
    property url iconSource
    property string text
    property string tip
    property color textColor: "#ececec"
    signal clicked()

    implicitHeight: 30
    implicitWidth: label.visible ? row.implicitWidth + 22 : 30
    radius: Math.min(6, theme.radius)
    color: mouse.pressed ? "#33ffffff" : mouse.containsMouse ? "#1fffffff" : "transparent"
    Behavior on color { ColorAnimation { duration: 90 } }

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 7
        Image {
            anchors.verticalCenter: parent.verticalCenter
            source: button.iconSource
            sourceSize: Qt.size(16, 16)
            width: 16
            height: 16
            opacity: 0.85
        }
        Label {
            id: label
            anchors.verticalCenter: parent.verticalCenter
            visible: button.text !== ""
            text: button.text
            color: button.textColor
            font.pixelSize: 13
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: button.clicked()
    }

    ToolTip.visible: mouse.containsMouse && tip !== ""
    ToolTip.delay: 600
    ToolTip.text: tip
}
