import QtQuick
import QtQuick.Layouts
import Quickshell
import Quickshell.Wayland
import "../components"
PanelWindow {
    id: panel
    required property var shell
    anchors { top: true; left: true; right: true }
    implicitHeight: 48
    exclusiveZone: 48
    color: "#161e2d"
    WlrLayershell.namespace: "ludash-panel"
    RowLayout {
        anchors.fill: parent; anchors.leftMargin: 16; anchors.rightMargin: 16; spacing: 10
        ShellButton { text: "◈  LUDASH"; onClicked: shell.launcherOpen = !shell.launcherOpen }
        Text { text: "QUICKSHELL / WAYLAND"; color: "#8ea3c2"; font.pixelSize: 10 }
        Item { Layout.fillWidth: true }
        Repeater {
            model: 4
            ShellButton { required property int index; text: String(index + 1); active: shell.state.workspace === index; onClicked: shell.command("workspace", index) }
        }
        Item { Layout.fillWidth: true }
        Text { id: clock; color: "#dce7f8"; font.pixelSize: 13 }
        Timer { interval: 1000; repeat: true; running: true; triggeredOnStart: true; onTriggered: clock.text = Qt.formatDateTime(new Date(), "MM.dd  HH:mm") }
        ShellButton { text: "⚙"; onClicked: shell.settingsOpen = !shell.settingsOpen }
        ShellButton { text: "⏻"; onClicked: shell.command("quit", "") }
    }
}
