import QtQuick
import QtQuick.Layouts
import Quickshell
import "../components"
import "../../components"
import "../../configuration"
import "../../effects"
import "../../style"

ColumnLayout {
    id: page
    required property var shell
    spacing: 16

    property var sessionHistory: []
    readonly property string bundledWallpaperPath: {
        const directory = Quickshell.env("LUNADASH_WALLPAPER_DIR") || ""
        return directory.length ? directory + "/florist.png" : ""
    }
    property string selectedWallpaper: shell.pendingWallpaper.length
        ? shell.pendingWallpaper
        : page.localPath(String(shell.state.wallpaperImage || ""))

    function localPath(value) {
        const text = String(value || "")
        return text.startsWith("file://") ? decodeURIComponent(text.slice(7)) : text
    }

    function fileUrl(value) {
        const text = String(value || "")
        if (!text.length)
            return ""
        return text.startsWith("file:") ? text : "file://" + text
    }

    function rememberLocally(path) {
        path = localPath(path)
        if (!path.length || path === bundledWallpaperPath)
            return
        const copy = sessionHistory.filter(entry => localPath(entry) !== path)
        copy.unshift(path)
        sessionHistory = copy.slice(0, 12)
    }

    readonly property var wallpaperCards: {
        const cards = []
        const seen = ({})
        const currentUrl = String(shell.state.wallpaperImage || "")
        const currentPath = page.localPath(currentUrl)
        const pending = String(shell.pendingWallpaper || "")
        const persisted = ((shell.state.appearance || {}).wallpaperHistory || [])
        const history = page.sessionHistory.concat(persisted)

        function addWallpaper(path, label, bundled) {
            path = page.localPath(path)
            if (!path.length || seen[path])
                return
            seen[path] = true
            cards.push({
                source: page.fileUrl(path),
                value: path,
                current: path === currentPath,
                add: false,
                bundled: Boolean(bundled),
                label: label || ""
            })
        }

        addWallpaper(page.bundledWallpaperPath, shell.tr("Default"), true)
        for (let i = 0; i < history.length; ++i)
            addWallpaper(history[i], shell.tr("Recent"), false)
        addWallpaper(currentPath, shell.tr("Current"), false)
        addWallpaper(pending, shell.tr("New"), false)
        cards.push({add: true, current: false, bundled: false, source: "", value: "", label: shell.tr("Add image")})
        return cards
    }

    onWallpaperCardsChanged: Qt.callLater(function() {
        if (wallpaperStrip.count <= 0)
            return
        let target = 0
        for (let i = 0; i < wallpaperCards.length; ++i) {
            if (wallpaperCards[i].current) {
                target = i
                break
            }
        }
        wallpaperStrip.currentIndex = target
        wallpaperStrip.positionViewAtIndex(target, ListView.Center)
    })

    Component.onCompleted: {
        const persisted = ((shell.state.appearance || {}).wallpaperHistory || [])
        sessionHistory = Array.from(persisted)
    }

    PageTitle { shell: page.shell; title: "Appearance" }

    SettingsCard {
        title: shell.tr("Wallpaper")
        description: shell.tr("Choose the bundled wallpaper, a recently used image, or add another image.")

        ListView {
            id: wallpaperStrip
            Layout.fillWidth: true
            Layout.preferredHeight: 186
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
                readonly property bool addCard: Boolean(modelData.add)
                readonly property bool currentCard: Boolean(modelData.current)
                width: addCard ? 150 : 236
                height: 168
                scale: wallpaperStrip.currentIndex === index ? 1 : 0.94
                opacity: wallpaperStrip.currentIndex === index ? 1 : 0.72
                Behavior on scale { NumberAnimation { duration: Math.max(180, Theme.motion); easing.type: Easing.OutCubic } }
                Behavior on opacity { NumberAnimation { duration: Theme.motion } }

                Rectangle {
                    anchors.fill: parent
                    radius: 18
                    color: card.addCard ? Theme.surface : Theme.control
                    border.width: wallpaperStrip.currentIndex === card.index ? 3 : 1
                    border.color: wallpaperStrip.currentIndex === card.index ? Theme.moon : Theme.border
                    clip: true

                    Image {
                        anchors.fill: parent
                        visible: !card.addCard
                        source: card.addCard ? "" : modelData.source
                        sourceSize.width: 640
                        sourceSize.height: 420
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        cache: false
                    }

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: 30
                        visible: !card.addCard
                        color: Qt.rgba(0.03, 0.05, 0.12, 0.72)
                        Text {
                            anchors.centerIn: parent
                            text: modelData.label || ""
                            color: Theme.moon
                            font.family: Theme.font
                            font.pixelSize: 11
                        }
                    }

                    Rectangle {
                        anchors.fill: parent
                        visible: !card.addCard && wallpaperStrip.currentIndex === card.index
                        color: Qt.rgba(Theme.starlight.r, Theme.starlight.g, Theme.starlight.b, 0.09)
                    }

                    Column {
                        anchors.centerIn: parent
                        visible: card.addCard
                        spacing: 4
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "☾"
                            color: Theme.moon
                            font.family: Theme.font
                            font.pixelSize: 36
                        }
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: shell.tr("Add image")
                            color: Theme.muted
                            font.family: Theme.font
                            font.pixelSize: 10
                        }
                    }

                    Rectangle {
                        visible: card.currentCard && !card.addCard
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 10
                        width: 12
                        height: 12
                        radius: 6
                        color: Theme.moon
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
                        if (card.addCard) {
                            page.shell.pickerPurpose = "wallpaper"
                            page.shell.pickerOpen = true
                        } else {
                            page.selectedWallpaper = String(modelData.value || "")
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
                    page.selectedWallpaper = page.bundledWallpaperPath
                }
            }
            ShellButton {
                text: shell.tr("Apply wallpaper")
                active: true
                enabled: page.selectedWallpaper.length > 0 && page.selectedWallpaper !== page.localPath(String(shell.state.wallpaperImage || ""))
                onClicked: {
                    page.rememberLocally(page.selectedWallpaper)
                    shell.command("wallpaper-image", page.selectedWallpaper)
                    shell.pendingWallpaper = ""
                }
            }
        }
    }

    AppearanceControls { shell: page.shell; Layout.fillWidth: true }
    EffectsControls { shell: page.shell; Layout.fillWidth: true }
}
