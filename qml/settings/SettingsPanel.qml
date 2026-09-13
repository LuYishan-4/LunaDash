import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Quickshell
import Quickshell.Wayland
import "../components"
import "../style"
import "../configuration"
import "../effects"
AnimatedPanel {
    id: settings
    required property var shell
    anchors { top: true; right: true }
    margins { top: Theme.barHeight + 12; right: 14 }
    implicitWidth: 500; implicitHeight: screen ? Math.min(700, screen.height - Theme.barHeight - 24) : 700
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Overlay
    WlrLayershell.namespace: "ludash-settings"
    color: "transparent"
    Rectangle { anchors.fill: parent; color: Theme.background; border.color: Theme.border; radius: Theme.radius }
    FileDialog {
        id: wallpaperDialog
        title: shell.tr("Choose wallpaper")
        nameFilters: ["Images (*.png *.jpg *.jpeg *.webp)"]
        onAccepted: shell.command("wallpaper-image", selectedFile.toString())
    }
    ScrollView {
        anchors.fill: parent; anchors.margins: 23; clip: true
        ColumnLayout {
        width: settings.width - 46; spacing: 12
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
        AppearanceControls { shell: settings.shell; Layout.fillWidth: true }
        EffectsControls { shell: settings.shell; Layout.fillWidth: true }
        ShellButton { text: shell.tr("Run an X11 application"); onClicked: { shell.settingsOpen = false; shell.x11Open = true } }
        Text { text: shell.tr((shell.state.network || {}).label || "Checking network"); color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        RowLayout {
            ShellButton { text: shell.tr("Configure network"); onClicked: shell.configureNetwork() }
            ShellButton { text: shell.tr("First-run guide"); onClicked: { shell.settingsOpen = false; shell.command("setup", "") } }
        }
        ShellButton { text: shell.tr("Input method and more"); onClicked: shell.launch("settings") }
        ShellButton { text: shell.tr("Manage metadata plugins"); onClicked: shell.launch("plugins") }
        Item { Layout.fillHeight: true }
    }
    }
}
