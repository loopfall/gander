import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// CSV and TSV as a table; the first row reads as the header.
Item {
    id: view
    property bool active
    property color textColor
    readonly property Flickable flickable: table

    TableView {
        id: table
        anchors.fill: parent
        model: view.active ? previewer.table : null
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        reuseItems: true
        columnWidthProvider: (column) => previewer.table.columnWidth(column)

        delegate: Rectangle {
            implicitHeight: 30
            color: row === 0 ? "#202026" : (row % 2 === 0 ? "#17171b" : "transparent")
            Text {
                anchors { fill: parent; leftMargin: 12; rightMargin: 8 }
                verticalAlignment: Text.AlignVCenter
                text: display !== undefined ? display : ""
                color: view.textColor
                font.pixelSize: 13
                font.weight: row === 0 ? Font.DemiBold : Font.Normal
                elide: Text.ElideRight
                textFormat: Text.PlainText
            }
            Rectangle {
                anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
                width: 1
                color: "#14ffffff"
            }
        }

        ScrollBar.vertical: ScrollBar {}
        ScrollBar.horizontal: ScrollBar {}
    }
}
