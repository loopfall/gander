import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// Plain text and code in a monospace face with light highlighting; Markdown
// rendered, in a readable column.
Item {
    id: view
    property var file: ({})
    property var details: ({})
    property bool active
    property bool markdown
    property color textColor
    property color dimColor
    readonly property Flickable flickable: flick
    readonly property string content: active && details.text !== undefined ? details.text : ""
    readonly property string language: markdown ? "plain" : (details.language || "plain")

    // After the text is in: the highlighter formats the whole document once.
    onContentChanged: Qt.callLater(applyHighlight)
    onLanguageChanged: Qt.callLater(applyHighlight)
    function applyHighlight() {
        if (view.markdown && view.content !== "")
            previewer.polishMarkdown(edit.textDocument)
        previewer.highlight(edit.textDocument, view.content === "" ? "plain" : view.language, theme.accent)
    }

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: view.markdown || view.details.longLines ? width : Math.max(width, edit.implicitWidth)
        contentHeight: edit.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        TextEdit {
            id: edit
            x: view.markdown ? Math.max(0, (flick.width - width) / 2) : 0
            width: view.markdown ? Math.min(flick.width, 780) : (view.details.longLines ? flick.width : Math.max(flick.width, implicitWidth))
            readOnly: true
            selectByMouse: true
            activeFocusOnPress: false
            persistentSelection: false
            textFormat: view.markdown ? TextEdit.MarkdownText : TextEdit.PlainText
            wrapMode: view.markdown || view.details.longLines ? TextEdit.Wrap : TextEdit.NoWrap
            color: view.textColor
            selectionColor: theme.accent
            selectedTextColor: theme.accentForeground
            font.family: view.markdown ? Qt.application.font.family : "monospace"
            font.pixelSize: view.markdown ? 15 : 13
            padding: view.markdown ? 36 : 18
            text: view.content
            baseUrl: view.file.dir ? "file://" + view.file.dir + "/" : ""
        }

        ScrollBar.vertical: ScrollBar {}
        ScrollBar.horizontal: ScrollBar {}
    }

    Rectangle {
        anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 12 }
        visible: view.active && view.details.truncated === true
        width: truncatedLabel.implicitWidth + 24
        height: 28
        radius: 14
        color: "#ee111114"
        border.color: "#26ffffff"
        Label {
            id: truncatedLabel
            anchors.centerIn: parent
            text: qsTr("Showing the beginning of a long file")
            color: view.dimColor
            font.pixelSize: 12
        }
    }

    Label {
        anchors.centerIn: parent
        visible: view.active && view.details.text === ""
        text: qsTr("Empty file")
        color: view.dimColor
        font.pixelSize: 14
    }
}
