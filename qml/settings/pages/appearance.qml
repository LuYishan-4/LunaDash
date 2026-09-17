import QtQuick
import QtQuick.Layouts
import "../components"
import "../../components"
import "../../configuration"
import "../../effects"
import "../../style"

ColumnLayout {
    id: page
    required property var shell
    spacing: 16

    property string selectedWallpaper: shell.pendingWallpaper.length
        ? shell.pendingWallpaper
        : String(shell.state.wallpaperImage || "")

    readonly property var wallpaperCards: {
        const cards = []
        const current = String(shell.state.wallpaperImage || "")
        const pending = String(shell.pendingWallpaper || "")
        if (current.length)
            cards.push({source: current, value: current, current: true})
        if (pending.length && pending !== current && ("file://" + pending) !== current)
            cards.push({source: pending.startsWith("file:") ? pending : "file://" + pending, value: pending, current: false})
        cards.push({add: true})
        return cards
    }

    onWallpaperCardsChanged: Qt.callLater(function() {
        if (wallpaperStrip.count > 0)
            wallpaperStrip.positionViewAtIndex(Math.max(0, wallpaperStrip.count - (shell.pendingWallpaper.length ? 2 : 1)), ListView.Center)
    })

    PageTitle { shell: page.shell; title: "Appearance" }

    SettingsCard {
        title: shell.tr("Wallpaper")
        description: shell.tr("Choose by preview. New images are staged first and only applied after confirmation.")

        ListView {
            id: wallpaperStrip
            Layout.fillWidth: true
            Layout.preferredHeight: 176
            orientation: ListView.Horizontal
            spacing: 14
            clip: true
            model: page.wallpaperCards
            boundsBehavior: Flickable.StopAtBounds
            snapMode: ListView.SnapToItem
            highlightMoveDuration: Math.max(220, Theme.motion * 2)
            highlightMoveVelocity: -1
            preferredHighlightBegin: width * 0.33
            preferredHighlightEnd: width * 0.67
            highlightRangeMode: ListView.ApplyRange

            delegate: Item {
                id: card
                required property var modelData
                required property int index
                width: modelData.add ? 150 : 236
                height: 158
                scale: wallpaperStrip.currentIndex === index ? 1 : 0.94
                opacity: wallpaperStrip.currentIndex === index ? 1 : 0.72
                Behavior on scale { NumberAnimation { duration: Math.max(180, Theme.motion); easing.type: Easing.OutCubic } }
                Behavior on opacity { NumberAnimation { duration: Theme.motion } }

                Rectangle {
                    anchors.fill: parent
                    radius: 18
                    color: modelData.add ? "#000000" : Theme.control
                    border.width: wallpaperStrip.currentIndex === card.index ? 3 : 1
                    border.color: wallpaperStrip.currentIndex === card.index ? Theme.accent : Theme.border
                    clip: true

                    Image {
                        anchors.fill: parent
                        visible: !modelData.add
                        source: modelData.add ? "" : modelData.source
                        sourceSize.width: 640
                        sourceSize.height: 420
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        cache: false
                    }

                    Rectangle {
                        anchors.fill: parent
                        visible: !modelData.add && wallpaperStrip.currentIndex === card.index
                        color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.08)
                    }

                    Text {
                        anchors.centerIn: parent
                        visible: modelData.add
                        text: "+"
                        color: Theme.text
                        font.family: Theme.font
                        font.pixelSize: 48
                        font.weight: Font.Light
                    }

                    Rectangle {
                        visible: modelData.current && !modelData.add
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 10
                        width: 12
                        height: 12
                        radius: 6
                        color: Theme.accent
                        border.width: 2
                        border.color: Theme.focusRing
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        wallpaperStrip.currentIndex = card.index
                        wallpaperStrip.positionViewAtIndex(card.index, ListView.Center)
                        if (modelData.add) {
                            page.shell.pickerOpen = true
                        } else {
                            page.selectedWallpaper = String(modelData.value)
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Text {
                Layout.fillWidth: true
                text: shell.pendingWallpaper.length
                    ? shell.tr("New wallpaper ready to apply")
                    : shell.tr("Select a preview or add another image")
                color: shell.pendingWallpaper.length ? Theme.accent : Theme.muted
                font.family: Theme.font
            }
            ShellButton {
                text: shell.tr("Reset to bundled wallpaper")
                onClicked: {
                    shell.pendingWallpaper = ""
                    shell.command("wallpaper-default", "")
                }
            }
            ShellButton {
                text: shell.tr("Apply wallpaper")
                active: true
                enabled: page.selectedWallpaper.length > 0 && page.selectedWallpaper !== String(shell.state.wallpaperImage || "")
                onClicked: {
                    shell.command("wallpaper-image", page.selectedWallpaper)
                    shell.pendingWallpaper = ""
                }
            }
        }
    }

    AppearanceControls { shell: page.shell; Layout.fillWidth: true }
    EffectsControls { shell: page.shell; Layout.fillWidth: true }
}
