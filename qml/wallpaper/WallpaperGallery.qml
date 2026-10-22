import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quickshell
import Quickshell.Wayland
import "../components"
import "../plugins"
import "../style"
import "GalleryModel.js" as GalleryModel

AnimatedPanel {
    id: gallery
    required property var shell
    property string collection: "library"
    property string category: ""
    property string kind: "all"
    property bool editDirectory: false
    readonly property var wallpapers: shell.state.wallpapers || ({})
    readonly property var entries: wallpapers.library || []
    readonly property var filtered: GalleryModel.visibleEntries(entries,
        wallpapers.resolvedDirectory || wallpapers.directory, collection,
        category, kind, search.text)
    readonly property var categories: [shell.tr("All categories")].concat(
        GalleryModel.categories(GalleryModel.visibleEntries(entries,
            wallpapers.resolvedDirectory || wallpapers.directory, collection, "", "all", "")))
    readonly property int columns: Math.max(1, Math.floor(grid.width / 180))
    readonly property string selectedPath: (wallpapers.current || {}).path || ""
    implicitWidth: Math.max(1, Math.min(920, (screen ? screen.width : 1440) - 40))
    implicitHeight: Math.max(1, Math.min(650,
        (screen ? screen.height : 900) - Theme.panelTopInset - Theme.panelBottomInset - 40))
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Overlay
    WlrLayershell.namespace: "lunadash-wallpaper-gallery"
    WlrLayershell.keyboardFocus: opened ? WlrKeyboardFocus.Exclusive : WlrKeyboardFocus.None
    color: "transparent"

    function select(index) {
        const entry = filtered[index]
        if (entry && entry.path !== selectedPath)
            shell.command("wallpaper-image", entry.path)
    }
    function move(delta) {
        grid.currentIndex = GalleryModel.nextIndex(grid.currentIndex, delta, grid.count)
        grid.positionViewAtIndex(grid.currentIndex, GridView.Contain)
    }
    function focusSearch() {
        if (!opened) return
        search.forceActiveFocus(Qt.ShortcutFocusReason)
        search.prepareInputMethod()
    }
    function closeGallery() { shell.wallpaperGalleryOpen = false }
    onOpenedChanged: if (opened) Qt.callLater(focusSearch)
    Component.onCompleted: Qt.callLater(focusSearch)
    onFilteredChanged: grid.currentIndex = filtered.length ? 0 : -1

    GlassSurface { anchors.fill: parent; shell: gallery.shell; radius: 24 }
    ExtensionSlot {
        anchors.fill: parent
        anchors.margins: 16
        shell: gallery.shell
        target: "wallpaper-gallery"
        context: ({ gallery: gallery, wallpapers: gallery.wallpapers })
        ColumnLayout {
            anchors.fill: parent
            spacing: 12
            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: shell.tr("Wallpapers")
                    color: Theme.text
                    font.family: Theme.font
                    font.pixelSize: 20
                    font.weight: Font.DemiBold
                }
                SoftField {
                    id: search
                    Layout.fillWidth: true
                    Layout.minimumWidth: 60
                    placeholderText: shell.tr("Search wallpapers…")
                    Accessible.name: placeholderText
                    Keys.onDownPressed: gallery.move(gallery.columns)
                    Keys.onUpPressed: gallery.move(-gallery.columns)
                    Keys.onReturnPressed: gallery.select(grid.currentIndex)
                    Keys.onEscapePressed: {
                        if (text.length) clear()
                        else gallery.closeGallery()
                    }
                }
                ShellButton {
                    iconName: "files"
                    Accessible.name: shell.tr("Library directory")
                    active: gallery.editDirectory
                    onClicked: gallery.editDirectory = !gallery.editDirectory
                }
                ShellButton {
                    text: "↻"
                    Accessible.name: shell.tr("Refresh wallpaper library")
                    onClicked: shell.wallpaperActions.run("refresh")
                }
                ShellButton {
                    text: "×"
                    Accessible.name: shell.tr("Close")
                    onClicked: gallery.closeGallery()
                }
            }
            GridLayout {
                Layout.fillWidth: true
                columns: gallery.width < 680 ? 1 : 2
                RowLayout {
                    Layout.fillWidth: true
                    Repeater {
                        model: [{key:"library", label:"Library"},
                            {key:"bundled", label:"Bundled"},
                            {key:"favorites", label:"Favorites"}]
                        ShellButton {
                            required property var modelData
                            Layout.fillWidth: true
                            text: shell.tr(modelData.label)
                            active: gallery.collection === modelData.key
                            onClicked: { gallery.collection = modelData.key; gallery.category = "" }
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Repeater {
                        model: ["dark", "light", "auto"]
                        ShellButton {
                            required property string modelData
                            Layout.fillWidth: true
                            text: shell.tr(modelData === "dark" ? "Dark" : modelData === "light" ? "Light" : "Auto")
                            active: (shell.state.appearance || {}).themeMode === modelData
                            onClicked: shell.setAppearance({themeMode: modelData})
                        }
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                visible: gallery.editDirectory
                SoftField {
                    id: directoryField
                    Layout.fillWidth: true
                    text: gallery.wallpapers.directory || ""
                    Accessible.name: shell.tr("Library directory")
                    onAccepted: {
                        const path = text.startsWith("~/")
                            ? Quickshell.env("HOME") + text.slice(1) : text
                        shell.setAppearance({wallpaperDirectory: path})
                    }
                }
                ShellButton {
                    text: shell.tr("Save directory")
                    onClicked: directoryField.accepted()
                }
                ShellButton {
                    text: shell.tr("Open folder")
                    enabled: gallery.wallpapers.directoryExists === true
                    onClicked: shell.openUrl(gallery.wallpapers.directoryUrl)
                }
            }
            RowLayout {
                Layout.fillWidth: true
                StyledComboBox {
                    Layout.fillWidth: true
                    model: gallery.categories
                    currentIndex: Math.max(0, gallery.categories.indexOf(gallery.category))
                    onActivated: index => gallery.category = index > 0 ? gallery.categories[index] : ""
                }
                StyledComboBox {
                    Layout.fillWidth: true
                    model: ["All wallpapers", "Static", "Live"]
                    translationContext: gallery.shell
                    onActivated: index => gallery.kind = ["all", "image", "video"][index]
                }
                ShellButton {
                    iconName: "shuffle"
                    text: shell.tr("Random")
                    enabled: gallery.filtered.length > 1
                    onClicked: {
                        const choices = gallery.filtered.map((entry, index) => index)
                            .filter(index => gallery.filtered[index].path !== gallery.selectedPath)
                        if (choices.length) gallery.select(choices[Math.floor(Math.random() * choices.length)])
                    }
                }
            }
            GridView {
                id: grid
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 0
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                cacheBuffer: 160
                cellWidth: width / gallery.columns
                cellHeight: cellWidth * 0.5625 + 34
                model: gallery.filtered
                currentIndex: count ? 0 : -1
                keyNavigationWraps: false
                ScrollBar.vertical: ScrollBar {}
                Keys.onReturnPressed: gallery.select(currentIndex)
                Keys.onEscapePressed: gallery.closeGallery()
                delegate: Item {
                    id: tile
                    required property var modelData
                    required property int index
                    width: grid.cellWidth
                    height: grid.cellHeight
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 5
                        color: tileMouse.containsMouse ? Theme.surfaceElevated : Theme.surfaceGlass
                        radius: 12
                        border.width: tile.modelData.path === gallery.selectedPath ? 2 : 1
                        border.color: tile.modelData.path === gallery.selectedPath
                            ? Theme.accent : tile.index === grid.currentIndex ? Theme.focusRing : Theme.hairline
                        clip: true
                        Image {
                            anchors.top: parent.top
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.margins: 3
                            height: Math.max(1, parent.height - 28)
                            // Video thumbnails are poster images; never give an mp4 to Image.
                            source: tile.modelData.preview || ""
                            sourceSize.width: 400
                            sourceSize.height: 225
                            asynchronous: true
                            fillMode: Image.PreserveAspectCrop
                        }
                        LineIcon {
                            anchors.centerIn: parent
                            visible: !tile.modelData.preview
                            name: tile.modelData.type === "video" ? "play" : "wallpaper"
                            width: 28
                            height: 28
                            ink: Theme.accent
                        }
                        Text {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.margins: 7
                            text: tile.modelData.name
                            textFormat: Text.PlainText
                            color: Theme.text
                            font.family: Theme.font
                            font.pixelSize: 11
                            elide: Text.ElideMiddle
                        }
                        MouseArea {
                            id: tileMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: { grid.currentIndex = tile.index; gallery.select(tile.index) }
                        }
                        ShellButton {
                            anchors.top: parent.top
                            anchors.right: parent.right
                            anchors.margins: 5
                            implicitWidth: 28
                            implicitHeight: 28
                            text: tile.modelData.favorite ? "★" : "☆"
                            active: tile.modelData.favorite === true
                            enabled: !shell.wallpaperActions.busy
                            Accessible.name: shell.tr("Favorite") + " " + tile.modelData.name
                            onClicked: shell.wallpaperActions.run("favorite", tile.modelData.path, !tile.modelData.favorite)
                        }
                    }
                }
                ColumnLayout {
                    anchors.centerIn: parent
                    width: Math.max(1, parent.width - 24)
                    visible: grid.count === 0
                    Text {
                        Layout.fillWidth: true
                        text: shell.tr("No matching wallpapers")
                        color: Theme.text
                        font.family: Theme.font
                        font.pixelSize: 18
                        horizontalAlignment: Text.AlignHCenter
                    }
                    Text {
                        Layout.fillWidth: true
                        text: shell.tr("Drop images or videos into your library folder, then press Super+W.")
                        color: Theme.muted
                        font.family: Theme.font
                        wrapMode: Text.Wrap
                        horizontalAlignment: Text.AlignHCenter
                    }
                    ShellButton {
                        Layout.alignment: Qt.AlignHCenter
                        visible: gallery.wallpapers.directoryExists !== true
                        text: shell.tr("Create library folder")
                        onClicked: shell.wallpaperActions.run("create-directory")
                    }
                }
            }
            Text {
                Layout.fillWidth: true
                text: gallery.filtered.length + " · " + (gallery.wallpapers.directory || "")
                textFormat: Text.PlainText
                color: Theme.muted
                font.family: Theme.font
                font.pixelSize: 10
                elide: Text.ElideMiddle
            }
        }
    }
}
