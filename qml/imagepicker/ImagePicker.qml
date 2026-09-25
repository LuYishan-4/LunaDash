import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.folderlistmodel
import Quickshell
import "../components"
import "../style"
import "../plugins"

ExtensionSlot {
    id: picker
    required shell
    target: "image-picker"
    context: ({
            picker: picker,
            purpose: shell.pickerPurpose
        })
    property bool opened: false
    property string folder: Quickshell.env("HOME") || "/"
    property string selectedPath: ""
    property real cornerRadius: 24
    property bool savingLauncher: false
    property string selectionError: ""
    signal closed

    readonly property string homePath: Quickshell.env("HOME") || "/"
    readonly property bool calendarMode: shell.pickerPurpose === "calendar"
    readonly property bool launcherMode: shell.pickerPurpose === "panel-launcher"
    readonly property bool imageOnly: calendarMode || launcherMode

    function parentDirectory() {
        const trimmed = String(folder).replace(/\/+$/, "");
        const index = trimmed.lastIndexOf("/");
        return index <= 0 ? "/" : trimmed.slice(0, index);
    }
    function normalizedLocalPath(value) {
        let path = String(value || "").trim()
        if (path.startsWith("file://")) {
            try { path = decodeURIComponent(path.slice(7)) }
            catch (error) { return "" }
        }
        if (!path.startsWith("/"))
            return ""
        return path.replace(/\/{2,}/g, "/")
    }
    function openDirectory(path) {
        const normalized = normalizedLocalPath(path)
        if (!normalized.length)
            return
        folder = normalized
        selectedPath = ""
        pathField.text = normalized
    }
    function applyTypedPath() {
        const normalized = normalizedLocalPath(pathField.text)
        if (!normalized.length) {
            pathField.invalid = true
            return
        }
        pathField.invalid = false
        const lower = normalized.toLowerCase()
        const imageFile = (imageOnly ? [".png", ".jpg", ".jpeg", ".webp", ".gif"].concat(launcherMode ? [".svg"] : []) : [".png", ".jpg", ".jpeg", ".webp", ".gif", ".mp4", ".webm", ".mkv", ".mov", ".m4v"]).some(suffix => lower.endsWith(suffix))
        if (imageFile) {
            const slash = normalized.lastIndexOf("/")
            folder = slash <= 0 ? "/" : normalized.slice(0, slash)
            selectedPath = normalized
        } else {
            openDirectory(normalized)
        }
    }
    function saveCalendarImage(path) {
        const document = JSON.parse(JSON.stringify((shell.state.shellModules || {}).document || {
            schemaVersion: 1,
            modules: {}
        }));
        if (!document.modules || !document.modules.overview)
            return false;
        if (!document.modules.overview.config)
            document.modules.overview.config = {};
        document.modules.overview.config.calendarImage = "file://" + path;
        shell.command("module-save", JSON.stringify(document));
        return true;
    }
    function acceptSelection() {
        if (!selectedPath.length)
            return;
        if (launcherMode) {
            const target = ((shell.state.settingsApi || {}).targets || []).find(item => item.id === "module:panel:config")
            if (!target || savingLauncher)
                return
            savingLauncher = true
            selectionError = ""
            const url = "file://" + selectedPath.split("/").map(part => encodeURIComponent(part)).join("/")
            shell.command("settings-update", JSON.stringify({
                target: target.id, revision: target.revision, changes: {launcherImage: url}
            }))
            return
        }
        if (calendarMode) {
            if (!saveCalendarImage(selectedPath))
                return;
            shell.pickerPurpose = "wallpaper";
            closed();
            shell.settingsOpen = false;
            shell.calendarOpen = true;
            return;
        }
        shell.pendingWallpaper = selectedPath;
        shell.pickerPurpose = "wallpaper";
        closed();
    }

    onOpenedChanged: if (opened) {
        folder = homePath;
        selectedPath = "";
        pathField.text = homePath;
        selectionError = "";
    }

    visible: opened
    focus: opened
    Keys.onEscapePressed: {
        const returnToCalendar = calendarMode;
        shell.pickerPurpose = "wallpaper";
        closed();
        if (returnToCalendar) {
            shell.settingsOpen = false;
            shell.calendarOpen = true;
        }
    }

    FolderListModel {
        id: entries
        folder: picker.opened ? "file://" + picker.folder : ""
        showDirs: true
        showDirsFirst: true
        showDotAndDotDot: false
        showHidden: false
        sortField: FolderListModel.Name
        nameFilters: picker.imageOnly ? ["*.png", "*.jpg", "*.jpeg", "*.webp", "*.gif", "*.PNG", "*.JPG", "*.JPEG", "*.WEBP", "*.GIF"].concat(picker.launcherMode ? ["*.svg", "*.SVG"] : []) : ["*.png", "*.jpg", "*.jpeg", "*.webp", "*.gif", "*.mp4", "*.webm", "*.mkv", "*.mov", "*.m4v", "*.PNG", "*.JPG", "*.JPEG", "*.WEBP", "*.GIF", "*.MP4", "*.WEBM", "*.MKV", "*.MOV", "*.M4V"]
    }

    Rectangle {
        anchors.fill: parent
        radius: picker.cornerRadius
        color: Qt.rgba(0, 0, 0, 0.64)
        MouseArea {
            anchors.fill: parent
            onClicked: {
                const returnToCalendar = picker.calendarMode;
                picker.shell.pickerPurpose = "wallpaper";
                picker.closed();
                if (returnToCalendar) {
                    picker.shell.settingsOpen = false;
                    picker.shell.calendarOpen = true;
                }
            }
        }
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
        Behavior on scale {
            NumberAnimation {
                duration: Theme.motion
                easing.type: Easing.OutCubic
            }
        }
        Behavior on opacity {
            NumberAnimation {
                duration: Theme.motion
            }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 12

            RowLayout {
                Layout.fillWidth: true
                LineIcon {
                    width: 20
                    height: 20
                    name: picker.calendarMode ? "dashboard" : "appearance"
                    ink: Theme.accent
                }
                Text {
                    Layout.fillWidth: true
                    text: picker.shell.tr(picker.calendarMode ? "Choose a calendar image" : picker.launcherMode ? "Choose launcher image" : "Choose a wallpaper")
                    color: Theme.text
                    font.family: Theme.font
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                }
                ShellButton {
                    text: "×"
                    Accessible.name: picker.shell.tr("Cancel")
                    onClicked: {
                        const returnToCalendar = picker.calendarMode;
                        picker.shell.pickerPurpose = "wallpaper";
                        picker.closed();
                        if (returnToCalendar) {
                            picker.shell.settingsOpen = false;
                            picker.shell.calendarOpen = true;
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                ShellButton {
                    text: picker.shell.tr("Up")
                    onClicked: picker.openDirectory(picker.parentDirectory())
                }
                ShellButton {
                    text: picker.shell.tr("Home")
                    onClicked: picker.openDirectory(picker.homePath)
                }
                ShellButton {
                    text: picker.shell.tr("Pictures")
                    onClicked: picker.openDirectory(picker.homePath + "/Pictures")
                }
                SoftField {
                    id: pathField
                    Layout.fillWidth: true
                    placeholderText: picker.shell.tr("Paste an absolute path or file:// URL")
                    text: picker.folder
                    clearButtonEnabled: false
                    Accessible.name: picker.shell.tr("Image path")
                    onAccepted: picker.applyTypedPath()
                    ToolTip.visible: hovered
                    ToolTip.text: picker.shell.tr("Paste a folder path or the full path of an image")
                }
                ShellButton {
                    text: picker.shell.tr("Go")
                    active: true
                    onClicked: picker.applyTypedPath()
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
                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                }
                delegate: Item {
                    required property var model
                    width: grid.cellWidth
                    height: grid.cellHeight
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 5
                        radius: 14
                        color: picker.selectedPath === model.filePath ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.20) : tileMouse.containsMouse ? Theme.controlHover : Theme.control
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
                                    visible: !model.fileIsDir && !/\.(mp4|webm|mkv|mov|m4v)$/i.test(model.filePath)
                                    source: visible ? "file://" + model.filePath : ""
                                    sourceSize.width: 320
                                    sourceSize.height: 220
                                    fillMode: Image.PreserveAspectCrop
                                    asynchronous: true
                                    smooth: true
                                }
                                Rectangle {
                                    anchors.fill: parent
                                    visible: model.fileIsDir || /\.(mp4|webm|mkv|mov|m4v)$/i.test(model.filePath)
                                    color: "transparent"
                                    LineIcon {
                                        anchors.centerIn: parent
                                        width: 34
                                        height: 34
                                        name: "files"
                                        ink: Theme.accent
                                    }
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
                        onDoubleClicked: if (!model.fileIsDir)
                            picker.acceptSelection()
                    }
                }
            }

            Text {
                visible: picker.selectionError.length > 0
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: picker.selectionError
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                color: Theme.danger
                font.family: Theme.font
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
                ShellButton {
                    text: picker.shell.tr("Cancel")
                    onClicked: {
                        const returnToCalendar = picker.calendarMode;
                        picker.shell.pickerPurpose = "wallpaper";
                        picker.closed();
                        if (returnToCalendar) {
                            picker.shell.settingsOpen = false;
                            picker.shell.calendarOpen = true;
                        }
                    }
                }
                ShellButton {
                    text: picker.shell.tr(picker.imageOnly ? "Use image" : "Add to wallpapers")
                    active: true
                    enabled: picker.selectedPath.length > 0 && !picker.savingLauncher
                    onClicked: picker.acceptSelection()
                }
            }
        }
    }
    Connections {
        target: picker.shell
        function onCommandCompleted(method, result) {
            if (method !== "settings-update" || !picker.savingLauncher || result.settingsTarget !== "module:panel:config")
                return
            picker.savingLauncher = false
            if (result.error) {
                picker.selectionError = picker.shell.tr(result.error)
                return
            }
            picker.shell.pickerPurpose = "wallpaper"
            picker.closed()
        }
    }
}
