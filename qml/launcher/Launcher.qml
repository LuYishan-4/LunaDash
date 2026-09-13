import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quickshell
import Quickshell.Wayland
import "../components"
PanelWindow {
    id: launcher
    required property var shell
    anchors { top: true; left: true }
    margins { top: 62; left: 16 }
    implicitWidth: 480; implicitHeight: 560
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Overlay
    WlrLayershell.keyboardFocus: WlrKeyboardFocus.Exclusive
    WlrLayershell.namespace: "ludash-launcher"
    color: "#1c293e"
    property var builtins: [
        { id: "files", name: "Files" }, { id: "console", name: "Console" },
        { id: "notes", name: "Notes" }, { id: "monitor", name: "System monitor" },
        { id: "packages", name: "Packages" }, { id: "plugins", name: "Plugins" }
    ]
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 20; spacing: 12
        RowLayout {
            TextField { id: search; Layout.fillWidth: true; placeholderText: shell.tr("Search applications…"); focus: true; Keys.onEscapePressed: shell.launcherOpen = false }
            ShellButton { text: "×"; onClicked: shell.launcherOpen = false }
        }
        ListView {
            Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 8
            model: launcher.builtins.filter(entry => entry.name.toLowerCase().includes(search.text.toLowerCase()))
            delegate: ShellButton { required property var modelData; width: ListView.view.width; text: shell.tr(modelData.name); onClicked: shell.launch(modelData.id) }
        }
        Text { text: shell.tr("Installed applications"); color: "#9bb0ce" }
        ListView {
            Layout.fillWidth: true; Layout.preferredHeight: 220; clip: true; spacing: 8
            model: DesktopEntries.applications.values.filter(entry => entry.name.toLowerCase().includes(search.text.toLowerCase()))
            delegate: ShellButton { required property var modelData; width: ListView.view.width; text: modelData.name; onClicked: { modelData.execute(); shell.launcherOpen = false } }
        }
    }
}
