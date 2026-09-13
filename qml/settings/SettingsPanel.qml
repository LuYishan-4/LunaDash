import QtQuick
import QtQuick.Layouts
import Quickshell
import Quickshell.Wayland
import "../components"
PanelWindow {
    required property var shell
    anchors { top: true; right: true }
    margins { top: 62; right: 16 }
    implicitWidth: 410; implicitHeight: 390
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Overlay
    WlrLayershell.namespace: "ludash-settings"
    color: "#1c293e"
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 24; spacing: 16
        RowLayout {
            Text { text: shell.tr("Desktop settings"); color: "#e5edfa"; font.pixelSize: 24; Layout.fillWidth: true }
            ShellButton { text: "×"; onClicked: shell.settingsOpen = false }
        }
        Text { text: shell.tr("Interface language"); color: "#9bb0ce" }
        RowLayout {
            ShellButton { text: shell.tr("Traditional Chinese"); active: shell.state.language === "zh_TW"; onClicked: shell.command("language", "zh_TW") }
            ShellButton { text: "English"; active: shell.state.language === "en_US"; onClicked: shell.command("language", "en_US") }
        }
        Text { text: shell.tr("Wallpaper"); color: "#9bb0ce" }
        RowLayout {
            ShellButton { text: shell.tr("Dusk"); onClicked: shell.command("wallpaper", 0) }
            ShellButton { text: shell.tr("Forest"); onClicked: shell.command("wallpaper", 1) }
        }
        ShellButton { text: shell.tr("Input method and more"); onClicked: shell.launch("settings") }
        ShellButton { text: shell.tr("Manage metadata plugins"); onClicked: shell.launch("plugins") }
        Item { Layout.fillHeight: true }
    }
}
