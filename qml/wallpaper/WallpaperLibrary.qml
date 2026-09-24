import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"
import "../settings/components"
import "../style"

SettingsCard {
    id: library
    required property var shell
    title: shell.tr("Wallpapers")
    description: shell.tr("Choose a static or live wallpaper. Folders inside your library become categories.")
    property string selected: ""
    property string category: ""
    property string kind: "all"
    readonly property var wallpaperState: shell.state.wallpapers || ({})
    readonly property var current: wallpaperState.current || ({})
    readonly property var entries: {
        const result = (wallpaperState.library || []).slice();
        const pending = shell.pendingWallpaper;
        if (pending && !result.some(entry => entry.path === pending)) {
            const video = /\.(mp4|webm|mkv|mov|m4v)$/i.test(pending);
            result.unshift({
                path: pending,
                url: "file://" + pending,
                preview: video ? "" : "file://" + pending,
                name: pending.split("/").pop(),
                category: "New",
                type: video ? "video" : "image"
            });
        }
        return result;
    }
    readonly property var categories: [shell.tr("All categories")].concat([...new Set(entries.map(entry => entry.category))])
    readonly property var filtered: entries.filter(entry => (!category || entry.category === category) && (kind === "all" || entry.type === kind) && String(entry.name + " " + entry.category).toLocaleLowerCase().includes(search.text.trim().toLocaleLowerCase()))
    readonly property var choice: entries.find(entry => entry.path === selected) || current

    Connections {
        target: library.shell
        function onPendingWallpaperChanged() {
            if (library.shell.pendingWallpaper)
                library.selected = library.shell.pendingWallpaper;
        }
    }
    RowLayout {
        Layout.fillWidth: true
        SoftField {
            id: search
            Layout.fillWidth: true
            placeholderText: shell.tr("Search wallpapers…")
        }
        ShellButton {
            text: shell.tr("Random")
            iconName: "shuffle"
            enabled: library.entries.length > 1
            onClicked: shell.command("wallpaper-random", "")
        }
    }
    GridLayout {
        Layout.fillWidth: true
        columns: library.width > 650 ? 2 : 1
        columnSpacing: 12
        StyledComboBox {
            Layout.fillWidth: true
            model: ["All wallpapers", "Static", "Live"]
            translationContext: library.shell
            onActivated: index => library.kind = ["all", "image", "video"][index]
        }
        StyledComboBox {
            Layout.fillWidth: true
            model: library.categories
            onActivated: index => library.category = index > 0 ? library.categories[index] : ""
        }
    }
    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: Math.max(140, Math.min(230, library.width * 0.28))
        radius: Theme.radiusMedium
        color: Theme.surfaceElevated
        border.color: Theme.hairline
        clip: true
        Image {
            anchors.fill: parent
            source: library.choice.preview || ""
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            sourceSize.width: 1000
            sourceSize.height: 560
        }
        LineIcon {
            visible: !library.choice.preview
            anchors.centerIn: parent
            width: 40
            height: 40
            name: "play"
            ink: Theme.accent
        }
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 42
            color: Theme.surfaceGlass
            RowLayout {
                anchors.fill: parent
                anchors.margins: 10
                Text {
                    Layout.fillWidth: true
                    text: library.choice.name || shell.tr("Wallpaper library")
                    color: Theme.text
                    font.family: Theme.font
                    elide: Text.ElideMiddle
                }
                Text {
                    text: library.choice.type === "video" ? shell.tr("Live") : shell.tr("Static")
                    color: Theme.accent
                    font.family: Theme.font
                }
            }
        }
    }
    GridView {
        id: grid
        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(288, Math.max(144, Math.ceil(count / Math.max(1, Math.floor(width / 170))) * 144))
        cellWidth: width / Math.max(1, Math.floor(width / 170))
        cellHeight: 144
        model: library.filtered
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}
        delegate: Rectangle {
            id: tile
            required property var modelData
            width: grid.cellWidth - 8
            height: grid.cellHeight - 8
            radius: Theme.radiusMedium
            color: Theme.surfaceElevated
            border.width: 2
            border.color: library.choice.path === modelData.path ? Theme.accent : Theme.hairline
            clip: true
            activeFocusOnTab: true
            Accessible.role: Accessible.Button
            Accessible.name: modelData.name
            Keys.onReturnPressed: library.selected = modelData.path
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 4
                spacing: 4
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    Image {
                        anchors.fill: parent
                        source: tile.modelData.preview || ""
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        sourceSize.width: 320
                        sourceSize.height: 180
                    }
                    LineIcon {
                        visible: tile.modelData.type === "video"
                        anchors.centerIn: parent
                        width: 24
                        height: 24
                        name: "play"
                        ink: Theme.accent
                    }
                }
                Text {
                    Layout.fillWidth: true
                    Layout.leftMargin: 6
                    Layout.rightMargin: 6
                    text: tile.modelData.name
                    color: Theme.text
                    font.family: Theme.font
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }
                Text {
                    Layout.fillWidth: true
                    Layout.leftMargin: 6
                    Layout.bottomMargin: 4
                    text: tile.modelData.current ? shell.tr("Current") : tile.modelData.category
                    color: Theme.muted
                    font.family: Theme.font
                    font.pixelSize: 10
                    elide: Text.ElideRight
                }
            }
            MouseArea {
                anchors.fill: parent
                onClicked: library.selected = tile.modelData.path
            }
        }
        Text {
            anchors.centerIn: parent
            visible: grid.count === 0
            text: shell.tr("No matching wallpapers")
            color: Theme.muted
            font.family: Theme.font
        }
    }
    RowLayout {
        Layout.fillWidth: true
        ShellButton {
            Layout.fillWidth: true
            text: shell.tr("Add image or video")
            iconName: "files"
            onClicked: {
                shell.pickerPurpose = "wallpaper";
                shell.pickerOpen = true;
            }
        }
        ShellButton {
            text: shell.tr("Use default")
            onClicked: shell.command("wallpaper-default", "")
        }
        ShellButton {
            text: shell.tr("Apply")
            active: true
            enabled: library.selected.length > 0 && library.selected !== library.current.path
            onClicked: shell.command("wallpaper-image", library.selected)
        }
    }
    Text {
        Layout.fillWidth: true
        text: shell.tr("Library directory")
        color: Theme.muted
        font.family: Theme.font
    }
    RowLayout {
        Layout.fillWidth: true
        SoftField {
            id: directory
            Layout.fillWidth: true
            text: library.wallpaperState.directory || ""
            placeholderText: shell.tr("Absolute folder path")
        }
        ShellButton {
            text: shell.tr("Save")
            onClicked: shell.setAppearance({
                wallpaperDirectory: directory.text
            })
        }
    }
    Text {
        Layout.fillWidth: true
        visible: Boolean(library.wallpaperState.error)
        text: shell.tr(library.wallpaperState.error || "")
        color: Theme.warning
        font.family: Theme.font
        wrapMode: Text.Wrap
    }
}
