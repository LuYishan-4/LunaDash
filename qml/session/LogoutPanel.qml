import QtQuick
import QtQuick.Layouts
import Quickshell
import Quickshell.Wayland
import "../components"
import "../style"
PanelWindow {
    required property var shell
    anchors { top: true; right: true }
    margins { top: 40; right: 14 }
    implicitWidth: 330; implicitHeight: 150
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Overlay
    WlrLayershell.namespace: "ludash-logout"
    color: "transparent"
    Rectangle { anchors.fill: parent; radius: 7; color: Theme.background; border.color: Theme.border }
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 22; spacing: 14
        Text { text: shell.tr("End this desktop session?"); color: Theme.text; font.family: Theme.font; font.pixelSize: 14 }
        RowLayout {
            ShellButton { text: shell.tr("Cancel"); onClicked: shell.logoutOpen = false }
            ShellButton { text: shell.tr("Log out"); onClicked: { shell.logoutOpen = false; shell.command("quit", "") } }
        }
    }
}
