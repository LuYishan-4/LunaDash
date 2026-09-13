import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Quickshell
import Quickshell.Wayland
import "../components"
import "../style"
PanelWindow {
    id: settings
    required property var shell
    anchors { top: true; right: true }
    margins { top: 40; right: 14 }
    implicitWidth: 420; implicitHeight: 430
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Overlay
    WlrLayershell.namespace: "ludash-settings"
    color: "transparent"
    Rectangle { anchors.fill: parent; color: Theme.background; border.color: Theme.border; radius: 7 }
    FileDialog {
        id: wallpaperDialog
        title: shell.tr("Choose wallpaper")
        nameFilters: ["Images (*.png *.jpg *.jpeg *.webp)"]
        onAccepted: shell.command("wallpaper-image", selectedFile.toString())
    }
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 23; spacing: 15
        RowLayout {
            Text { text: shell.tr("Desktop settings"); color: Theme.text; font.family: Theme.font; font.pixelSize: 20; Layout.fillWidth: true }
            ShellButton { text: "×"; onClicked: shell.settingsOpen = false }
        }
        Text { text: shell.tr("Interface language"); color: Theme.muted; font.family: Theme.font }
        RowLayout {
            ShellButton { text: shell.tr("Traditional Chinese"); active: shell.state.language === "zh_TW"; onClicked: shell.command("language", "zh_TW") }
            ShellButton { text: "English"; active: shell.state.language === "en_US"; onClicked: shell.command("language", "en_US") }
        }
        Text { text: shell.tr("Wallpaper"); color: Theme.muted; font.family: Theme.font }
        RowLayout {
            ShellButton { text: shell.tr("Florist"); active: Boolean(shell.state.wallpaperImage); onClicked: shell.command("wallpaper-default", "") }
            ShellButton { text: shell.tr("Choose image…"); onClicked: wallpaperDialog.open() }
        }
        RowLayout {
            ShellButton { text: shell.tr("Dusk"); onClicked: shell.command("wallpaper", 0) }
            ShellButton { text: shell.tr("Forest"); onClicked: shell.command("wallpaper", 1) }
        }
        ShellButton { text: shell.tr("Desktop information"); active: shell.overviewOpen; onClicked: shell.overviewOpen = !shell.overviewOpen }
        ShellButton { text: shell.tr("Input method and more"); onClicked: shell.launch("settings") }
        ShellButton { text: shell.tr("Manage metadata plugins"); onClicked: shell.launch("plugins") }
        Item { Layout.fillHeight: true }
    }
}
