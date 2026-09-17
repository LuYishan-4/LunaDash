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

    readonly property string bundledWallpaperPath: {
        const directory = Quickshell.env("LUNADASH_WALLPAPER_DIR") || ""
        return directory.length ? directory + "/florist.png" : ""
    }
    readonly property string currentWallpaper: localPath(String(shell.state.wallpaperImage || ""))
    property string selectedWallpaper: currentWallpaper

    function localPath(value) {
        const text = String(value || "")
        return text.startsWith("file://") ? decodeURIComponent(text.slice(7)) : text
    }

    function fileUrl(value) {
        const text = localPath(value)
        return text.length ? "file://" + text : ""
    }

    function choose(path) {
        const normalized = localPath(path)
        if (!normalized.length)
            return
        selectedWallpaper = normalized
    }

    function applySelected() {
        if (!selectedWallpaper.length || selectedWallpaper === currentWallpaper)
            return
        shell.wallpaperOverride = fileUrl(selectedWallpaper)
        shell.command("wallpaper-image", selectedWallpaper)
        shell.pendingWallpaper = ""
    }

    readonly property var wallpaperCards: {
        const cards = []
        const seen = ({})
        const history = ((shell.state.appearance || {}).wallpaperHistory || [])
        const pending = localPath(shell.pendingWallpaper)

        function add(path, role) {
            path = page.localPath(path)
            if (!path.length || seen[path])
                return
            seen[path] = true
            cards.push({
                path: path,
                source: page.fileUrl(path),
                role: role,
                current: path === page.currentWallpaper
            })
        }

        add(page.bundledWallpaperPath, "default")
        add(page.currentWallpaper, "current")
        for (let i = 0; i < history.length; ++i)
            add(history[i], "recent")
        add(pending, "new")
        return cards
    }

    onCurrentWallpaperChanged: {
        if (!selectedWallpaper.length || shell.wallpaperOverride.length === 0)
            selectedWallpaper = currentWallpaper
    }

    Connections {
        target: shell
        function onPendingWallpaperChanged() {
            if (shell.pendingWallpaper.length)
                page.choose(shell.pendingWallpaper)
        }
    }

    PageTitle { shell: page.shell; title: "Appearance" }

    SettingsCard {
        title: shell.tr("Wallpaper")
        description: shell.tr("Pick a wallpaper, preview the selection, then apply it once. LunaDash keeps recently used images here.")

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 250
            spacing: 14

            Rectangle {
                Layout.preferredWidth: 360
                Layout.fillHeight: true
                radius: 20
                color: Theme.surface
                border.width: 1
                border.color: Theme.starlight
                clip: true

                Image {
                    anchors.fill: parent
                    source: page.fileUrl(page.selectedWallpaper)
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    sourceSize.width: 900
                    sourceSize.height: 600
                    cache: false
                }

                Rectangle {
                    anchors.fill: parent
                    color: Qt.rgba(0.02, 0.03, 0.10, 0.16)
                }

                Column {
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    anchors.margins: 14
                    spacing: 2
                    Text {
                        text: page.selectedWallpaper === page.currentWallpaper
                            ? shell.tr("Current wallpaper")
                            : shell.tr("Selected wallpaper")
                        color: Theme.moon
                        font.family: Theme.font
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                    }
                    Text {
                        width: 320
                        text: page.selectedWallpaper.split("/").pop()
                        color: Theme.text
                        font.family: Theme.font
                        font.pixelSize: 11
                        elide: Text.ElideMiddle
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 10

                Text {
                    text: shell.tr("Wallpaper library")
                    color: Theme.text
                    font.family: Theme.font
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                }

                ListView {
                    id: wallpaperStrip
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    orientation: ListView.Horizontal
                    spacing: 10
                    clip: true
                    model: page.wallpaperCards
                    boundsBehavior: Flickable.StopAtBounds

                    delegate: Item {
                        id: card
                        required property var modelData
                        width: 150
                        height: wallpaperStrip.height

                        Rectangle {
                            anchors.fill: parent
                            radius: 16
                            color: Theme.control
                            border.width: page.selectedWallpaper === modelData.path ? 2 : 1
                            border.color: page.selectedWallpaper === modelData.path ? Theme.moon : Theme.border
                            clip: true

                            Image {
                                anchors.fill: parent
                                source: modelData.source
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                                sourceSize.width: 420
                                sourceSize.height: 280
                                cache: false
                            }

                            Rectangle {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                height: 28
                                color: Qt.rgba(0.02, 0.03, 0.10, 0.76)
                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.current
                                        ? shell.tr("Current")
                                        : modelData.role === "default"
                                            ? shell.tr("Default")
                                            : modelData.role === "new"
                                                ? shell.tr("New")
                                                : shell.tr("Recent")
                                    color: modelData.current ? Theme.moon : Theme.text
                                    font.family: Theme.font
                                    font.pixelSize: 10
                                }
                            }

                            Text {
                                visible: page.selectedWallpaper === modelData.path
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: 8
                                text: "✦"
                                color: Theme.moon
                                font.pixelSize: 16
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: page.choose(modelData.path)
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    ShellButton {
                        text: "☾  " + shell.tr("Choose image")
                        onClicked: {
                            shell.pickerPurpose = "wallpaper"
                            shell.pickerOpen = true
                        }
                    }
                    Item { Layout.fillWidth: true }
                    ShellButton {
                        text: shell.tr("Use default")
                        enabled: page.bundledWallpaperPath.length > 0 && page.currentWallpaper !== page.bundledWallpaperPath
                        onClicked: {
                            page.choose(page.bundledWallpaperPath)
                            page.applySelected()
                        }
                    }
                    ShellButton {
                        text: shell.tr("Apply")
                        active: true
                        enabled: page.selectedWallpaper.length > 0 && page.selectedWallpaper !== page.currentWallpaper
                        onClicked: page.applySelected()
                    }
                }
            }
        }
    }

    AppearanceControls { shell: page.shell; Layout.fillWidth: true }
    EffectsControls { shell: page.shell; Layout.fillWidth: true }
}
