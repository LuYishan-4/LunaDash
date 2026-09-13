import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quickshell
import Quickshell.Wayland
import "../components"
import "../style"
AnimatedPanel {
    id: launcher
    required property var shell
    anchors { top: true; left: true }
    margins { top: Theme.barHeight + 12; left: 14 }
    implicitWidth: 480; implicitHeight: screen ? Math.min(610, screen.height - 56) : 610
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Overlay
    WlrLayershell.keyboardFocus: WlrKeyboardFocus.Exclusive
    WlrLayershell.namespace: "ludash-launcher"
    color: "transparent"
    Rectangle { anchors.fill: parent; radius: Theme.radius; color: Theme.background; border.color: Theme.border }
    property var builtins: [
        { id: "files", name: "Files" }, { id: "console", name: "Console" },
        { id: "settings", name: "Settings" }, { id: "monitor", name: "System monitor" },
        { id: "packages", name: "Packages" }, { id: "plugins", name: "Plugins" }
    ]
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 20; spacing: 12
        RowLayout {
            TextField {
                id: search; Layout.fillWidth: true; placeholderText: shell.tr("Search applications...")
                color: Theme.text; placeholderTextColor: Theme.muted; font.family: Theme.font
                background: Rectangle { color: "#162224"; border.color: search.activeFocus ? Theme.accent : Theme.border; radius: 3 }
                focus: true; Keys.onEscapePressed: shell.launcherOpen = false
            }
            ShellButton { text: "×"; onClicked: shell.launcherOpen = false }
        }
        ShellButton { text: shell.tr("Run an X11 application"); Layout.fillWidth: true; onClicked: { shell.launcherOpen = false; shell.x11Open = true } }
        GridLayout {
            columns: 2; Layout.fillWidth: true; rowSpacing: 7; columnSpacing: 7
            Repeater {
                model: launcher.builtins.filter(entry => shell.tr(entry.name).toLowerCase().includes(search.text.toLowerCase()))
                ShellButton { required property var modelData; Layout.fillWidth: true; text: shell.tr(modelData.name); onClicked: shell.launch(modelData.id) }
            }
        }
        Text { text: shell.tr("Open windows"); color: Theme.accent; font.family: Theme.font }
        ListView {
            Layout.fillWidth: true; Layout.preferredHeight: 120; clip: true; spacing: 6
            model: shell.state.clients.filter(client => !client.desktop)
            delegate: ShellButton {
                required property var modelData
                width: ListView.view.width; text: (modelData.workspace + 1) + "  " + modelData.title.slice(0, 38); active: modelData.focused
                onClicked: { shell.command("focus", modelData.id); shell.launcherOpen = false }
            }
            Text { anchors.centerIn: parent; visible: parent.count === 0; text: shell.tr("No open windows"); color: Theme.muted; font.family: Theme.font; font.pixelSize: 12 }
        }
        Text { text: shell.tr("Installed applications"); color: Theme.accent; font.family: Theme.font }
        ListView {
            Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 6
            model: DesktopEntries.applications.values.filter(entry => entry.name.toLowerCase().includes(search.text.toLowerCase()))
            delegate: ShellButton { required property var modelData; width: ListView.view.width; text: modelData.name.slice(0, 42); onClicked: { modelData.execute(); shell.launcherOpen = false } }
        }
    }
}
