import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtMultimedia

import "Format.js" as Format

// Plays at once, with sound, like Quick Look. Nautilus's thumbnail stands in
// until the first decoded frame; controls appear on hover or when paused.
Item {
    id: view
    property var file: ({})
    property bool active
    property MediaPlayer player
    property AudioOutput audio
    property alias output: videoOutput

    readonly property size resolution: {
        const r = player ? player.metaData.value(MediaMetaData.Resolution) : undefined
        return r && r.width > 0 ? r : Qt.size(0, 0)
    }
    readonly property real aspect: resolution.width > 0 ? resolution.width / resolution.height
        : poster.implicitWidth > 0 ? poster.implicitWidth / poster.implicitHeight : 16 / 9
    readonly property bool playing: player && player.playbackState === MediaPlayer.PlayingState
    readonly property bool hasFrame: player && player.hasVideo && player.position > 0

    Rectangle {
        anchors.fill: parent
        color: "#000000"
    }

    Image {
        id: poster
        anchors.fill: parent
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        source: view.active && view.file.thumbnail ? view.file.thumbnail : ""
        visible: !view.hasFrame
    }

    VideoOutput {
        id: videoOutput
        anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectFit
    }

    HoverHandler { id: hover }
    TapHandler {
        onTapped: view.playing ? view.player.pause() : view.player.play()
    }

    Rectangle {
        id: controls
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 12 }
        height: 40
        radius: Math.min(8, theme.radius + 4)
        color: "#cc111114"
        opacity: hover.hovered || !view.playing ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 160 } }

        Row {
            id: left
            anchors { left: parent.left; leftMargin: 8; verticalCenter: parent.verticalCenter }
            spacing: 6
            HeaderButton {
                iconSource: view.playing ? "qrc:/icons/pause.svg" : "qrc:/icons/play.svg"
                tip: qsTr("Play or pause (K)")
                onClicked: view.playing ? view.player.pause() : view.player.play()
            }
            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: Format.time(view.player ? view.player.position : 0)
                color: "#ececec"
                font.pixelSize: 12
                font.features: { "tnum": 1 }
            }
        }

        Slider {
            id: seek
            anchors { left: left.right; right: right.left; leftMargin: 8; rightMargin: 8; verticalCenter: parent.verticalCenter }
            from: 0
            to: view.player && view.player.duration > 0 ? view.player.duration : 1
            value: view.player ? view.player.position : 0
            enabled: view.player && view.player.seekable
            onMoved: view.player.position = value
        }

        Row {
            id: right
            anchors { right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
            spacing: 6
            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: Format.time(view.player ? view.player.duration : 0)
                color: "#9b9ba3"
                font.pixelSize: 12
                font.features: { "tnum": 1 }
            }
            HeaderButton {
                iconSource: view.audio && view.audio.muted ? "qrc:/icons/mute.svg" : "qrc:/icons/volume.svg"
                tip: qsTr("Mute (M)")
                onClicked: view.audio.muted = !view.audio.muted
            }
        }
    }

    Label {
        anchors.centerIn: parent
        visible: view.active && view.player && view.player.error !== MediaPlayer.NoError
        text: view.player ? view.player.errorString : ""
        color: "#ececec"
        wrapMode: Text.Wrap
        width: parent.width * 0.7
        horizontalAlignment: Text.AlignHCenter
    }
}
