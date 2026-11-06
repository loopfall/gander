import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Effects
import QtMultimedia

import "Format.js" as Format

// Cover, title, artist and a waveform you can click to seek. The backdrop
// takes its colour from the cover.
Item {
    id: view
    property var file: ({})
    property var details: ({})
    property bool active
    property MediaPlayer player
    property color textColor
    property color dimColor
    property color cardColor

    readonly property bool playing: player && player.playbackState === MediaPlayer.PlayingState
    readonly property real duration: player && player.duration > 0 ? player.duration : (details.durationMs || 0)
    readonly property real progress: duration > 0 && player ? player.position / duration : 0
    readonly property string title: details.title || (file.name ? file.name.replace(/\.[^.]+$/, "") : "")

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0; color: view.details.tint || view.cardColor; Behavior on color { ColorAnimation { duration: 220 } } }
            GradientStop { position: 1; color: view.cardColor }
        }
    }

    Item {
        id: coverBox
        anchors { left: parent.left; top: parent.top; bottom: parent.bottom; margins: 28 }
        width: height

        RectangularShadow {
            anchors.fill: cover
            radius: Math.min(8, theme.radius + 4)
            blur: 28
            offset.y: 8
            color: "#90000000"
            visible: cover.status === Image.Ready
        }
        Image {
            id: cover
            anchors.fill: parent
            source: view.active && view.details.cover ? view.details.cover : ""
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            smooth: true
            mipmap: true
            visible: status === Image.Ready
            opacity: visible ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: 140 } }
        }
        Rectangle {
            anchors.fill: parent
            visible: cover.status !== Image.Ready
            radius: Math.min(8, theme.radius + 4)
            color: "#1effffff"
            Image {
                anchors.centerIn: parent
                width: parent.width * 0.42
                height: width
                sourceSize: Qt.size(128, 128)
                source: "image://gander/icon/audio-x-generic"
                opacity: 0.6
            }
        }
    }

    Column {
        anchors { left: coverBox.right; leftMargin: 28; right: parent.right; rightMargin: 32; verticalCenter: parent.verticalCenter }
        spacing: 6

        Label {
            width: parent.width
            text: view.title
            color: view.textColor
            font.pixelSize: 24
            font.weight: Font.Bold
            elide: Text.ElideRight
            maximumLineCount: 2
            wrapMode: Text.Wrap
        }
        Label {
            width: parent.width
            visible: text !== ""
            text: view.details.artist || ""
            color: view.textColor
            opacity: 0.85
            font.pixelSize: 16
            elide: Text.ElideRight
        }
        Label {
            width: parent.width
            visible: text !== ""
            text: [view.details.album || "", view.details.year ? String(view.details.year) : ""].filter(s => s !== "").join("  ·  ")
            color: view.dimColor
            font.pixelSize: 13
            elide: Text.ElideRight
        }

        Item { width: 1; height: 14 }

        Item {
            id: wave
            width: parent.width
            height: 52
            readonly property var bars: view.details.waveform || []

            // Before the waveform arrives (a fraction of a second), a plain
            // progress line holds its place.
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width
                height: 3
                radius: 1.5
                color: "#33ffffff"
                visible: wave.bars.length === 0
                Rectangle {
                    width: parent.width * view.progress
                    height: parent.height
                    radius: 1.5
                    color: theme.accent
                }
            }

            Row {
                id: barRow
                anchors.fill: parent
                clip: true
                readonly property real slot: wave.bars.length > 0 ? wave.width / wave.bars.length : 1
                spacing: slot * 0.4
                opacity: wave.bars.length > 0 ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: 200 } }
                Repeater {
                    model: wave.bars
                    Rectangle {
                        required property var modelData
                        required property int index
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.max(1, barRow.slot * 0.6)
                        height: Math.max(2, modelData * wave.height)
                        radius: width / 2
                        color: (index + 0.5) / wave.bars.length <= view.progress ? theme.accent : "#4dffffff"
                    }
                }
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                enabled: view.player && view.player.seekable
                onClicked: (mouse) => view.player.position = view.duration * (mouse.x / width)
            }
        }

        Item {
            width: parent.width
            height: 34
            HeaderButton {
                id: play
                anchors.verticalCenter: parent.verticalCenter
                iconSource: view.playing ? "qrc:/icons/pause.svg" : "qrc:/icons/play.svg"
                tip: qsTr("Play or pause (K)")
                onClicked: view.playing ? view.player.pause() : view.player.play()
            }
            Label {
                anchors { left: play.right; leftMargin: 8; verticalCenter: parent.verticalCenter }
                text: Format.time(view.player ? view.player.position : 0) + "  /  " + Format.time(view.duration)
                color: view.dimColor
                font.pixelSize: 12
                font.features: { "tnum": 1 }
            }
        }
    }
}
