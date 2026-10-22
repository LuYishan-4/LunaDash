import QtQuick
import QtQuick.Layouts
import "../components"
import "../style"
import "Lyrics.js" as Lyrics

Rectangle {
    id: view
    required property var shell
    property var media: ({available: false, players: []})
    property string preferredService: ""
    property string preferredBus: ""
    property string errorMessage: ""
    property bool showLyrics: true
    property real position: 0
    property double snapshotTime: 0
    readonly property var lyricData: Lyrics.parse(media.lyrics || "")
    readonly property int lyricIndex: Lyrics.currentIndex(lyricData.timed, position)
    readonly property real length: Math.max(0, Number(media.lengthUs || 0) / 1000000)
    readonly property var players: media.players || []
    signal action(string name, string value)
    signal playerSelected(string service, string bus)
    color: Theme.surfaceOpaque
    radius: 22
    clip: true

    function updatePosition() {
        const elapsed = media.playing && media.positionSupported ? Math.max(0, Date.now() - snapshotTime) / 1000 : 0
        position = Math.max(0, Number(media.positionUs || 0) / 1000000 + elapsed * Number(media.rate || 1))
        if (length > 0)
            position = Math.min(length, position)
    }
    onMediaChanged: { snapshotTime = Date.now(); updatePosition() }
    Timer { interval: 250; repeat: true; running: view.visible && Boolean(view.media.playing); onTriggered: view.updatePosition() }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: view.width < 700 ? 16 : 28
        spacing: 12
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: view.width < 700 ? 12 : 26
            MediaDisc {
                Layout.preferredWidth: Math.max(130, Math.min(270, view.width * 0.26))
                Layout.preferredHeight: width
                Layout.alignment: Qt.AlignVCenter
                artwork: view.media.artUrl || ""
                playing: Boolean(view.media.playing)
                progress: view.length > 0 ? view.position / view.length : 0
            }
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 160
                spacing: 8
                Item { Layout.fillHeight: true; Layout.maximumHeight: 20 }
                Text {
                    Layout.fillWidth: true
                    text: view.media.available ? (view.media.title || shell.tr("Unknown track")) : shell.tr("Nothing is playing")
                    textFormat: Text.PlainText
                    horizontalAlignment: Text.AlignHCenter
                    color: Theme.accent
                    font.family: Theme.font
                    font.pixelSize: 21
                    font.weight: Font.DemiBold
                    wrapMode: Text.Wrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                }
                Text {
                    Layout.fillWidth: true
                    text: view.media.artist || (view.media.available ? view.media.album || "" : shell.tr("Open a player and start playback."))
                    textFormat: Text.PlainText
                    horizontalAlignment: Text.AlignHCenter
                    color: Theme.success
                    font.family: Theme.font
                    font.pixelSize: 13
                    wrapMode: Text.Wrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                }
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    Column {
                        anchors.centerIn: parent
                        width: parent.width
                        spacing: 16
                        visible: view.showLyrics && view.lyricData.timed.length > 0
                        Repeater {
                            model: 5
                            Text {
                                required property int index
                                readonly property int row: Math.max(0, view.lyricIndex) + index - 2
                                width: parent.width
                                height: 28
                                text: row >= 0 && row < view.lyricData.timed.length ? view.lyricData.timed[row].text : ""
                                textFormat: Text.PlainText
                                color: row === view.lyricIndex ? Theme.accent : Theme.muted
                                opacity: row === view.lyricIndex ? 1 : index === 0 || index === 4 ? 0.28 : 0.65
                                font.family: Theme.font
                                font.pixelSize: row === view.lyricIndex ? 19 : 16
                                font.weight: row === view.lyricIndex ? Font.DemiBold : Font.Normal
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                                Behavior on opacity { NumberAnimation { duration: Theme.motion } }
                                Behavior on color { ColorAnimation { duration: Theme.motion } }
                            }
                        }
                    }
                    Flickable {
                        anchors.fill: parent
                        contentHeight: plainLyrics.implicitHeight
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds
                        visible: view.showLyrics && !view.lyricData.timed.length && view.lyricData.plain.length > 0
                        Text {
                            id: plainLyrics
                            width: parent.width
                            text: view.lyricData.plain
                            textFormat: Text.PlainText
                            wrapMode: Text.Wrap
                            horizontalAlignment: Text.AlignHCenter
                            lineHeight: 1.6
                            color: Theme.text
                            font.family: Theme.font
                            font.pixelSize: 17
                        }
                    }
                    Column {
                        anchors.centerIn: parent
                        width: parent.width
                        spacing: 12
                        visible: !view.showLyrics || (!view.lyricData.timed.length && !view.lyricData.plain.length)
                        LineIcon { anchors.horizontalCenter: parent.horizontalCenter; width: 30; height: 30; name: "music"; ink: Theme.starlight; opacity: 0.55 }
                        Text {
                            width: parent.width
                            text: !view.media.available ? shell.tr("Play something in Spotify, YouTube, VLC, or another MPRIS player.")
                                : view.showLyrics ? shell.tr("Lyrics are not provided by this player.") : view.media.album || view.media.identity || ""
                            textFormat: Text.PlainText
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.Wrap
                            color: Theme.muted
                            font.family: Theme.font
                            font.pixelSize: 13
                        }
                    }
                }
            }
            MediaMascot {
                visible: view.width >= 720
                Layout.preferredWidth: Math.min(210, view.width * 0.22)
                Layout.preferredHeight: width
                Layout.alignment: Qt.AlignVCenter
                playing: Boolean(view.media.playing)
            }
        }
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 8
            ShellButton {
                iconName: "shuffle"; quiet: true; active: Boolean(view.media.shuffle)
                enabled: view.media.available && Boolean(view.media.shuffleSupported)
                toolTip: shell.tr("Shuffle"); Accessible.name: toolTip
                onClicked: view.action("shuffle", view.media.shuffle ? "false" : "true")
            }
            ShellButton {
                iconName: "previous"; quiet: true; enabled: view.media.available && Boolean(view.media.canGoPrevious)
                toolTip: shell.tr("Previous track"); Accessible.name: toolTip
                onClicked: view.action("previous", "")
            }
            ShellButton {
                Layout.preferredWidth: 60; Layout.preferredHeight: 48
                iconName: view.media.playing ? "pause" : "play"; active: true
                enabled: view.media.available && Boolean(view.media.playing ? view.media.canPause : view.media.canPlay)
                toolTip: view.media.playing ? shell.tr("Pause") : shell.tr("Play"); Accessible.name: toolTip
                onClicked: view.action("play-pause", "")
            }
            ShellButton {
                iconName: "next"; quiet: true; enabled: view.media.available && Boolean(view.media.canGoNext)
                toolTip: shell.tr("Next track"); Accessible.name: toolTip
                onClicked: view.action("next", "")
            }
            ShellButton {
                iconName: "lyrics"; quiet: true; active: view.showLyrics
                toolTip: shell.tr("Lyrics"); Accessible.name: toolTip
                onClicked: view.showLyrics = !view.showLyrics
            }
            ShellButton {
                iconName: "repeat"; quiet: true; active: (view.media.loopStatus || "None") !== "None"
                text: view.media.loopStatus === "Track" ? "1" : ""
                enabled: view.media.available && Boolean(view.media.loopSupported)
                toolTip: shell.tr("Repeat"); Accessible.name: toolTip
                onClicked: view.action("loop", view.media.loopStatus === "None" ? "Playlist" : view.media.loopStatus === "Playlist" ? "Track" : "None")
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: view.width < 700 ? 0 : 80
            Layout.rightMargin: view.width < 700 ? 0 : 80
            spacing: 10
            Text { text: Lyrics.clock(view.position); color: Theme.muted; font.family: Theme.font; font.pixelSize: 11 }
            SoftSlider {
                id: seek
                property real requestedValue: value
                Layout.fillWidth: true
                from: 0; to: Math.max(1, view.length)
                enabled: view.media.available && Boolean(view.media.canSeek) && view.length > 0 && Boolean(view.media.trackId)
                displayValue: Lyrics.clock(value)
                Accessible.name: shell.tr("Playback position")
                onPressedChanged: if (!pressed && enabled) view.action("seek", String(Math.round(requestedValue * 1000000)))
                onMoved: {
                    requestedValue = value
                    if (!pressed) view.action("seek", String(Math.round(value * 1000000)))
                }
                Binding { target: seek; property: "value"; value: view.position; when: !seek.pressed }
            }
            Text { text: Lyrics.clock(view.length); color: Theme.muted; font.family: Theme.font; font.pixelSize: 11 }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            LineIcon { width: 18; height: 18; name: "sound"; ink: Theme.accent }
            SoftSlider {
                id: volume
                property real requestedValue: value
                Layout.preferredWidth: Math.min(180, view.width * 0.24)
                from: 0; to: 1; stepSize: 0.01
                enabled: view.media.available && Boolean(view.media.volumeSupported)
                displayValue: Math.round(value * 100) + "%"
                Accessible.name: shell.tr("Player volume")
                onPressedChanged: if (!pressed && enabled) view.action("volume", String(requestedValue))
                onMoved: {
                    requestedValue = value
                    if (!pressed) view.action("volume", String(value))
                }
                Binding { target: volume; property: "value"; value: Number(view.media.volume || 0); when: !volume.pressed }
            }
            Item { Layout.fillWidth: true }
            StyledComboBox {
                id: playerChoice
                Layout.preferredWidth: Math.min(260, view.width * 0.38)
                model: [shell.tr("Follow playing media")].concat(view.players.map(p => p.identity))
                currentIndex: view.preferredService.length ? view.players.findIndex(p => p.service === view.preferredService && p.bus === view.preferredBus) + 1 : 0
                displayText: currentIndex === 0 && view.media.available ? view.media.identity : currentText
                enabled: view.players.length > 0
                Accessible.name: shell.tr("Media player")
                onActivated: index => {
                    const player = index > 0 ? view.players[index - 1] : null
                    view.playerSelected(player ? player.service : "", player ? player.bus : "")
                }
            }
            ShellButton {
                iconName: "windows"; quiet: true
                enabled: view.media.available && Boolean(view.media.canRaise)
                toolTip: shell.tr("Show player window"); Accessible.name: toolTip
                onClicked: view.action("raise", "")
            }
        }
        Text {
            Layout.fillWidth: true
            visible: view.errorMessage.length > 0
            text: view.errorMessage
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            maximumLineCount: 2
            elide: Text.ElideRight
            color: Theme.warning
            font.family: Theme.font
            font.pixelSize: 11
        }
    }
}
