import QtQuick
import QtQuick.Layouts
import Quickshell.Io
import "../components"
import "../style"

Rectangle {
    id: card
    required property var shell
    property var media: ({available:false})
    color: Theme.surface
    radius: 20
    border.width: 1
    border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.24)
    clip: true

    function run(action) {
        if (mediaProcess.running)
            return
        const service = String(card.media.service || "")
        mediaProcess.command = action
            ? [shell.shellToolExecutable, "media-action", action, service]
            : [shell.shellToolExecutable, "media-status", service]
        mediaProcess.running = true
    }

    function adopt(text) {
        if (!text.trim())
            return
        try {
            const object = JSON.parse(text)
            if (!object.error)
                media = object
        } catch (error) {
            console.warn("Could not parse MPRIS status: " + error)
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 13

        Rectangle {
            Layout.preferredWidth: Math.min(130, card.height - 24)
            Layout.preferredHeight: Layout.preferredWidth
            radius: 16
            color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.12)
            clip: true

            Image {
                id: artwork
                anchors.fill: parent
                source: card.media.artUrl || ""
                sourceSize.width: Math.max(160, width * 2)
                sourceSize.height: Math.max(160, height * 2)
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                cache: false
                visible: card.media.available && source.toString().length > 0 && status !== Image.Error
            }
            Rectangle {
                anchors.fill: parent
                visible: !artwork.visible
                color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.10)
                LunaDashLogo {
                    anchors.centerIn: parent
                    width: parent.width * 0.58
                    height: width
                    animated: false
                    primaryColor: Theme.accent
                    secondaryColor: Qt.lighter(Theme.accent, 1.2)
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 4

            Text {
                Layout.fillWidth: true
                text: card.media.available ? (card.media.title || shell.tr("Unknown track")) : shell.tr("Nothing is playing")
                color: Theme.text
                font.family: Theme.font
                font.pixelSize: 17
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
            Text {
                Layout.fillWidth: true
                text: card.media.available
                    ? [card.media.artist, card.media.album].filter(Boolean).join("  ·  ")
                    : shell.tr("Play something in Spotify, YouTube, VLC, or another MPRIS player.")
                color: Theme.muted
                font.family: Theme.font
                font.pixelSize: 11
                elide: Text.ElideRight
            }
            Text {
                Layout.fillWidth: true
                visible: card.media.available
                text: card.media.identity || shell.tr("Media player")
                color: Theme.accent
                font.family: Theme.font
                font.pixelSize: 10
                elide: Text.ElideRight
            }
            Item { Layout.fillHeight: true }
            RowLayout {
                Layout.fillWidth: true
                spacing: 7
                ShellButton {
                    text: "‹‹"
                    enabled: card.media.available && (card.media.canGoPrevious ?? true)
                    Accessible.name: shell.tr("Previous track")
                    onClicked: card.run("previous")
                }
                ShellButton {
                    text: card.media.playing ? "Ⅱ" : "▶"
                    active: card.media.playing ?? false
                    enabled: card.media.available && ((card.media.canPlay ?? true) || (card.media.canPause ?? true))
                    Accessible.name: card.media.playing ? shell.tr("Pause") : shell.tr("Play")
                    onClicked: card.run("play-pause")
                }
                ShellButton {
                    text: "››"
                    enabled: card.media.available && (card.media.canGoNext ?? true)
                    Accessible.name: shell.tr("Next track")
                    onClicked: card.run("next")
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: card.media.playbackStatus || ""
                    color: Theme.muted
                    font.family: Theme.font
                    font.pixelSize: 10
                }
            }
        }
    }

    Process {
        id: mediaProcess
        command: [card.shell.shellToolExecutable, "media-status"]
        stdout: StdioCollector { onStreamFinished: card.adopt(text) }
        stderr: StdioCollector { onStreamFinished: if (text.trim()) console.warn(text.trim()) }
        onExited: (code, status) => {
            if (code === 0 && mediaProcess.command[1] !== "media-status")
                refreshTimer.restart()
        }
    }

    Timer {
        id: refreshTimer
        interval: 1200
        repeat: true
        running: card.visible
        triggeredOnStart: true
        onTriggered: if (!mediaProcess.running) card.run("")
    }
}
