import QtQuick
import Quickshell
import Quickshell.Wayland
import "../components"
import "../style"
PanelWindow {
    id: panel
    required property var shell
    anchors { top: true; left: true; right: true }
    implicitHeight: Theme.barHeight
    exclusiveZone: Theme.barHeight
    color: "transparent"
    WlrLayershell.namespace: "ludash-panel"
    property var stats: shell.state.system || ({})
    Rectangle { anchors.fill: parent; color: Theme.background; radius: 18 }
    Row {
        anchors { left: parent.left; leftMargin: 10; verticalCenter: parent.verticalCenter } spacing: 5
        Segment { text: "✦"; fill: Theme.accent; ink: "#102133"; onClicked: shell.launcherOpen = !shell.launcherOpen }
        Repeater {
            model: (shell.state.appearance || {}).workspaceCount || 4
            Segment { required property int index; text: String(index + 1); implicitWidth: panel.width < 1100 ? 24 : shell.state.workspace === index ? 44 : 30; selected: shell.state.workspace === index; onClicked: shell.command("workspace", index) }
        }
        Segment { text: "⌘"; onClicked: shell.launch("console") }
    }
    Row {
        anchors.centerIn: parent; spacing: 5
        Segment { text: "◈"; ink: Theme.accent; onClicked: shell.setAppearance({ overview: !shell.overviewOpen }) }
        Segment { text: shell.focusedTitle.slice(0, panel.width > 1100 ? 32 : 16); fill: Theme.surface; onClicked: shell.launcherOpen = !shell.launcherOpen }
    }
    Row {
        anchors { right: parent.right; rightMargin: 10; verticalCenter: parent.verticalCenter } spacing: 4
        Segment { visible: panel.width > 1250; text: "CPU  " + (panel.stats.cpuPercent || 0) + "%"; ink: Theme.muted; onClicked: shell.launch("monitor") }
        Segment {
            id: clock; property string time: ""; text: time
            onClicked: shell.setAppearance({ overview: !shell.overviewOpen })
            Timer { interval: 1000; repeat: true; running: true; triggeredOnStart: true; onTriggered: clock.time = Qt.formatDateTime(new Date(), Theme.clock24Hour ? "ddd  HH:mm" : "ddd  h:mm AP") }
        }
        Segment { text: (shell.state.network || {}).connected ? "↔" : "×"; ink: (shell.state.network || {}).internet ? Theme.accent : Theme.muted; onClicked: shell.settingsOpen = !shell.settingsOpen }
        Segment { visible: (panel.stats.batteryPercent ?? -1) >= 0; text: panel.stats.batteryPercent + "%"; onClicked: shell.launch("monitor") }
        Segment { text: "⚙"; fill: Theme.surface; onClicked: shell.settingsOpen = !shell.settingsOpen }
        Segment { text: "⏻"; ink: Theme.danger; onClicked: shell.logoutOpen = !shell.logoutOpen }
    }
}
