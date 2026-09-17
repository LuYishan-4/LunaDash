import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.folderlistmodel
import Quickshell
import "../components"
import "../style"

Item {
    id: picker
    required property var shell
    property bool opened: false
    property string folder: Quickshell.env("HOME") || "/"
    property string selectedPath: ""
    property real cornerRadius: 24
    signal closed()

    readonly property string homePath: Quickshell.env("HOME") || "/"

    function parentDirectory() {
        const trimmed = String(folder).replace(/\/+$/, "")
        const index = trimmed.lastIndexOf("/")
        return index <= 0 ? "/" : trimmed.slice(0, index)
    }
    function openDirectory(path) {
        folder = path
        selectedPath = ""
    }
    function acceptSelection() {
        if (!selectedPath.length)
            return
        shell.pendingWallpaper = selectedPath
        closed()
    }

    onOpenedChanged: if (opened) {
        folder = homePath
        selectedPath = ""
    }

    visible: opened
    focus: opened
    Keys.onEscapePressed: closed()

    FolderListModel {
        id: entries
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
        color: Qt.rgba(0, 0, 0, 0.64)
        MouseArea { anchors.fill: parent; onClicked: picker.closed() }
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 28
        radius: 22
        color: Theme.surfaceOpaque
        border.width: 1
        border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.38)
        scale: picker.opened ? 1 : 0.96
        opacity: picker.opened ? 1 : 0
        Behavior on scale { NumberAnimation { duration: Theme.motion; easing.type: Easing.OutCubic } }
        Behavior on opacity { NumberAnimation { duration: Theme.motion } }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 12

            RowLayout {
                Layout.fillWidth: true
                LineIcon { width: 20; height: 20; name: "appearance"; ink: Theme.accent }
                Text {
                    Layout.fillWidth: true
                    text: picker.shell.tr("Choose a wallpaper image")
                    color: Theme.text
                    font.family: Theme.font
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                }
                ShellButton { text: "×"; Accessible.name: picker.shell.tr("Cancel"); onClicked: picker.closed() }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                ShellButton { text: picker.shell.tr("Up"); onClicked: picker.openDirectory(picker.parentDirectory()) }
                ShellButton { text: picker.shell.tr("Home"); onClicked: picker.openDirectory(picker.homePath) }
                ShellButton { text: picker.shell.tr("Pictures"); onClicked: picker.openDirectory(picker.homePath + "/Pictures") }
                Text {
                    Layout.fillWidth: true
                    text: picker.folder
                    color: Theme.muted
                    font.family: Theme.font
                    font.pixelSize: 11
                    elide: Text.ElideLeft
                    horizontalAlignment: Text.AlignRight
                }
            }

            GridView {
                id: grid
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: entries
                cellWidth: 146
                cellHeight: 140
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                delegate: Item {
                    required property var model
                    width: grid.cellWidth
                    height: grid.cellHeight
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 5
                        radius: 14
                        color: picker.selectedPath === model.filePath
                            ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.20)
                            : tileMouse.containsMouse ? Theme.controlHover : Theme.control
                        border.width: picker.selectedPath === model.filePath ? 2 : 1
                        border.color: picker.selectedPath === model.filePath ? Theme.accent : Theme.border
                        clip: true

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 0

                            Item {
                                Layout.fillWidth: true
                                Layout.fillHeight: true

                                Image {
                                    anchors.fill: parent
                                    visible: !model.fileIsDir
                                    source: model.fileIsDir ? "" : "file://" + model.filePath
                                    sourceSize.width: 320
                                    sourceSize.height: 220
                                    fillMode: Image.PreserveAspectCrop
                                    asynchronous: true
                                    smooth: true
                                }
                                Rectangle {
                                    anchors.fill: parent
                                    visible: model.fileIsDir
                                    color: "transparent"
                                    LineIcon { anchors.centerIn: parent; width: 34; height: 34; name: "files"; ink: Theme.accent }
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 30
                                Layout.leftMargin: 8
                                Layout.rightMargin: 8
                                text: model.fileName
                                color: Theme.text
                                font.family: Theme.font
                                font.pixelSize: 11
                                verticalAlignment: Text.AlignVCenter
                                horizontalAlignment: Text.AlignHCenter
                                elide: Text.ElideMiddle
                                maximumLineCount: 1
                            }
                        }
                    }
                    MouseArea {
                        id: tileMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: model.fileIsDir ? picker.openDirectory(model.filePath) : picker.selectedPath = model.filePath
                        onDoubleClicked: if (!model.fileIsDir) picker.acceptSelection()
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Text {
                    Layout.fillWidth: true
                    text: picker.selectedPath.length ? picker.selectedPath.split("/").pop() : picker.shell.tr("Select an image")
                    color: picker.selectedPath.length ? Theme.text : Theme.muted
                    font.family: Theme.font
                    elide: Text.ElideMiddle
                }
                ShellButton { text: picker.shell.tr("Cancel"); onClicked: picker.closed() }
                ShellButton {
                    text: picker.shell.tr("Add to wallpapers")
                    active: true
                    enabled: picker.selectedPath.length > 0
                    onClicked: picker.acceptSelection()
                }
            }
        }
    }
}
