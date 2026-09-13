import QtQuick
import QtQuick.Layouts
import Quickshell
import Quickshell.Wayland
import "../components"
PanelWindow {
    required property var shell
    anchors { bottom: true; left: true; right: true }
    implicitHeight: 78
    exclusiveZone: 78
    color: "#182337"
    WlrLayershell.namespace: "ludash-dock"
    RowLayout {
        anchors.fill: parent; anchors.margins: 14; spacing: 9
        ShellButton { text: "▱ " + shell.tr("Files"); onClicked: shell.launch("files") }
        ShellButton { text: "⌘ " + shell.tr("Console"); onClicked: shell.launch("console") }
        ShellButton { text: "✎ " + shell.tr("Notes"); onClicked: shell.launch("notes") }
        ShellButton { text: "⬡ " + shell.tr("Packages"); onClicked: shell.launch("packages") }
        Rectangle { width: 1; height: 30; color: "#415372" }
        Repeater {
            model: shell.state.clients.filter(client => !client.desktop && client.workspace === shell.state.workspace)
            ShellButton { required property var modelData; text: modelData.title.slice(0, 24); active: modelData.focused; onClicked: shell.command("focus", modelData.id) }
        }
        Item { Layout.fillWidth: true }
    }
}
