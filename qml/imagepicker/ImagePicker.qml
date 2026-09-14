import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.folderlistmodel
import Quickshell
import "../components"
import "../style"

// Wallpaper picker drawn inside the settings surface. It shares the settings
// layer-shell surface, keyboard focus and visual style, so it always appears
// above the settings content instead of opening a separate window behind it.
Item {
    id: picker
    required property var shell
    property bool opened: false
    property string folder: Quickshell.env("HOME") || "/"
    property string selectedPath: ""
    property bool gridView: true
    property real cornerRadius: 24
    signal closed()

    readonly property var imageExtensions: ["png", "jpg", "jpeg", "webp"]
    readonly property int maximumBytes: 64 * 1024 * 1024
    readonly property string homePath: Quickshell.env("HOME") || "/"

    function extensionOf(name) {
        const parts = String(name).split(".")
        return parts.length > 1 ? parts[parts.length - 1].toLowerCase() : ""
    }
    function isImageName(name) {
        return picker.imageExtensions.indexOf(picker.extensionOf(name)) >= 0
    }
    function formatBytes(bytes) {
        if (bytes >= 1024 * 1024) return (bytes / (1024 * 1024)).toFixed(1) + " MiB"
        if (bytes >= 1024) return Math.round(bytes / 1024) + " KiB"
        return bytes + " B"
    }
    function openDirectory(path) { picker.folder = path; picker.selectedPath = "" }
    function parentDirectory() {
        const trimmed = String(picker.folder).replace(/\/+$/, "")
        const index = trimmed.lastIndexOf("/")
        return index <= 0 ? "/" : trimmed.slice(0, index)
    }
    function select(path) { picker.selectedPath = path }
    function useSelected() {
        if (picker.selectedPath.length === 0) return
        picker.shell.command("wallpaper-image", picker.selectedPath)
        picker.close()
    }
    function close() { picker.closed() }

    onOpenedChanged: if (opened) { selectedPath = ""; folder = homePath }

    visible: opened
    focus: opened
    Keys.onEscapePressed: picker.close()

    FolderListModel {
        id: entries
        // Only scan while the picker is on screen.
        folder: picker.opened ? "file://" + picker.folder : ""
        showDirs: true
        showDirsFirst: true
        showDotAndDotDot: false
        showHidden: false
        sortField: FolderListModel.Name
        nameFilters: ["*.png", "*.jpg", "*.jpeg", "*.webp", "*.PNG", "*.JPG", "*.JPEG", "*.WEBP"]
    }

    Rectangle {
        anchors.fill: parent
        radius: picker.cornerRadius
        color: Qt.rgba(0, 0, 0, 0.55)
        MouseArea { anchors.fill: parent; acceptedButtons: Qt.LeftButton; onClicked: picker.close() }
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 26
        radius: 16
        color: Theme.background
        border.width: 1
        border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.24)

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 12

            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                LineIcon { Layout.preferredWidth: 20; Layout.preferredHeight: 20; name: "files"; ink: Theme.accent }
                Text {
                    Layout.fillWidth: true
                    text: picker.shell.tr("Choose a wallpaper image")
                    color: Theme.text; font.family: Theme.font; font.pixelSize: 18; font.bold: true
                }
                ShellButton { text: "×"; Accessible.name: picker.shell.tr("Cancel"); onClicked: picker.close() }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                ShellButton { text: picker.shell.tr("Up"); onClicked: picker.openDirectory(picker.parentDirectory()) }
                ShellButton { text: picker.shell.tr("Home"); onClicked: picker.openDirectory(picker.homePath) }
                ShellButton { text: picker.shell.tr("Pictures"); onClicked: picker.openDirectory(picker.homePath + "/Pictures") }
                ShellButton { text: picker.gridView ? picker.shell.tr("List view") : picker.shell.tr("Grid view"); onClicked: picker.gridView = !picker.gridView }
                Text {
                    Layout.fillWidth: true
                    text: picker.folder
                    color: Theme.muted; font.family: Theme.font; font.pixelSize: 11
                    elide: Text.ElideLeft
                    verticalAlignment: Text.AlignVCenter
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 16

                ListView {
                    visible: !picker.gridView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: entries
                    spacing: 2
                    ScrollBar.vertical: ScrollBar {}
                    delegate: Rectangle {
                        required property var model
                        width: ListView.view.width
                        height: 38
                        radius: 8
                        color: picker.selectedPath === model.filePath
                            ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.2)
                            : rowMouse.containsMouse ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.1) : "transparent"
                        Row {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.leftMargin: 10
                            spacing: 10
                            LineIcon { width: 17; height: 17; name: model.fileIsDir ? "files" : "appearance"; ink: model.fileIsDir ? Theme.accent : Theme.muted; anchors.verticalCenter: parent.verticalCenter }
                            Text { text: model.fileName; color: Theme.text; font.family: Theme.font; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
                            Text { visible: !model.fileIsDir; text: picker.formatBytes(model.fileSize); color: Theme.muted; font.pixelSize: 10; anchors.verticalCenter: parent.verticalCenter }
                        }
                        MouseArea {
                            id: rowMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: model.fileIsDir ? picker.openDirectory(model.filePath) : picker.select(model.filePath)
                            onDoubleClicked: if (!model.fileIsDir) picker.useSelected()
                        }
                    }
                }

                GridView {
                    id: fileGrid
                    visible: picker.gridView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: entries
                    cellWidth: 108
                    cellHeight: 112
                    ScrollBar.vertical: ScrollBar {}
                    delegate: Item {
                        required property var model
                        width: fileGrid.cellWidth
                        height: fileGrid.cellHeight
                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: 5
                            radius: 10
                            color: picker.selectedPath === model.filePath
                                ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.22)
                                : cellMouse.containsMouse ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.1) : Qt.rgba(Theme.surface.r, Theme.surface.g, Theme.surface.b, 0.6)
                            border.width: picker.selectedPath === model.filePath ? 1 : 0
                            border.color: Theme.accent

                            Image {
                                anchors.fill: parent
                                anchors.margins: model.fileIsDir ? 26 : 8
                                anchors.bottomMargin: model.fileIsDir ? 26 : 22
                                visible: !model.fileIsDir
                                source: model.fileIsDir ? "" : "file://" + model.filePath
                                sourceSize.width: 96
                                sourceSize.height: 96
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                                smooth: true
                            }
                            LineIcon {
                                anchors.centerIn: parent
                                visible: model.fileIsDir
                                width: 30; height: 30
                                name: "files"; ink: Theme.accent
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 4
                                width: parent.width - 10
                                text: model.fileName
                                color: Theme.muted
                                font.pixelSize: 9
                                elide: Text.ElideMiddle
                                horizontalAlignment: Text.AlignHCenter
                            }
                        }
                        MouseArea {
                            id: cellMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: model.fileIsDir ? picker.openDirectory(model.filePath) : picker.select(model.filePath)
                            onDoubleClicked: if (!model.fileIsDir) picker.useSelected()
                        }
                    }
                }

                Rectangle {
                    Layout.preferredWidth: 284
                    Layout.fillHeight: true
                    radius: 12
                    color: Qt.rgba(Theme.surface.r, Theme.surface.g, Theme.surface.b, 0.6)
                    border.width: 1
                    border.color: Theme.border

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 10

                        Text { text: picker.shell.tr("Preview"); color: Theme.text; font.family: Theme.font; font.pixelSize: 13; font.bold: true }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 170
                            radius: 10
                            color: Qt.rgba(0, 0, 0, 0.35)
                            Image {
                                anchors.fill: parent
                                anchors.margins: 6
                                source: picker.selectedPath.length > 0 ? "file://" + picker.selectedPath : ""
                                sourceSize.width: 512
                                sourceSize.height: 512
                                fillMode: Image.PreserveAspectFit
                                asynchronous: true
                                smooth: true
                            }
                            Text {
                                anchors.centerIn: parent
                                visible: picker.selectedPath.length === 0
                                text: picker.shell.tr("Select an image")
                                color: Theme.muted; font.pixelSize: 11
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: picker.selectedPath.length > 0 ? picker.selectedPath.split("/").pop() : picker.shell.tr("No file selected")
                            color: Theme.text; font.family: Theme.font; font.pixelSize: 12
                            wrapMode: Text.WrapAnywhere
                        }
                        Text {
                            Layout.fillWidth: true
                            text: picker.shell.tr("Readable PNG, JPEG or WebP files up to 64 MiB. Larger or corrupt images are rejected when applied.")
                            color: Theme.muted; font.pixelSize: 10; wrapMode: Text.WordWrap
                        }
                        Item { Layout.fillHeight: true }
                        ShellButton { Layout.fillWidth: true; text: picker.shell.tr("Use this image"); active: picker.selectedPath.length > 0; enabled: picker.selectedPath.length > 0; onClicked: picker.useSelected() }
                        ShellButton { Layout.fillWidth: true; text: picker.shell.tr("Cancel"); onClicked: picker.close() }
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                visible: entries.count === 0
                text: picker.shell.tr("No folders or supported images here.")
                color: Theme.muted; font.family: Theme.font; font.pixelSize: 11
            }
        }
    }
}
