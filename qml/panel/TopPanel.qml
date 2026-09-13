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
    Row {
        anchors.left: parent.left; height: parent.height; spacing: -6
        Segment { text: "◇"; fill: "#132023"; ink: Theme.accent; onClicked: shell.launcherOpen = !shell.launcherOpen }
        Repeater {
            model: 4
            Segment { required property int index; text: shell.state.workspace === index ? "◆" : "·"; implicitWidth: 29; selected: shell.state.workspace === index; onClicked: shell.command("workspace", index) }
        }
        Segment { text: "~"; onClicked: shell.launch("console") }
        Segment { visible: panel.width > 1250; text: shell.focusedTitle.slice(0, 20); onClicked: shell.launcherOpen = !shell.launcherOpen }
    }
    Row {
        anchors.horizontalCenter: parent.horizontalCenter; height: parent.height; spacing: -5
        Segment { text: "▱"; ink: Theme.muted; onClicked: shell.launch("files") }
        Segment { text: "✎"; fill: Theme.lavender; ink: "#293238"; onClicked: shell.launch("notes") }
        Segment { text: "◈  LuDash"; fill: Theme.accent; ink: "#182526"; onClicked: shell.overviewOpen = !shell.overviewOpen }
        Segment {
            id: clock; property string time: ""; text: "◷ " + time; fill: "#a8c6c4"; ink: "#243437"
            onClicked: shell.overviewOpen = !shell.overviewOpen
            Timer { interval: 1000; repeat: true; running: true; triggeredOnStart: true; onTriggered: clock.time = Qt.formatDateTime(new Date(), "HH:mm") }
        }
        Rectangle {
            width: panel.width > 1100 ? 100 : 62; height: Theme.barHeight; color: Theme.background
            Row {
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 4 }
                spacing: 1
                Repeater {
                    model: panel.stats.cpuHistory || []
                    Rectangle { required property var modelData; width: 3; height: Math.max(2, Number(modelData) / 100 * 19); anchors.bottom: parent.bottom; color: Theme.accent }
                }
            }
            MouseArea { anchors.fill: parent; onClicked: shell.launch("monitor") }
        }
    }
    Row {
        anchors.right: parent.right; height: parent.height; spacing: -5
        Segment { visible: panel.width > 1100; text: "⬡"; ink: Theme.lavender; onClicked: shell.launch("packages") }
        Segment { text: "CPU " + (panel.stats.cpuPercent || 0) + "%"; ink: Theme.accent; onClicked: shell.launch("monitor") }
        Segment { text: "MEM " + (panel.stats.memoryPercent || 0) + "%"; ink: Theme.muted; onClicked: shell.launch("monitor") }
        Segment { visible: (panel.stats.batteryPercent ?? -1) >= 0; text: "▰ " + panel.stats.batteryPercent + "%"; ink: Theme.muted; onClicked: shell.overviewOpen = !shell.overviewOpen }
        Segment { text: "⚙"; onClicked: shell.settingsOpen = !shell.settingsOpen }
        Segment { text: "⏻"; ink: Theme.danger; onClicked: shell.logoutOpen = !shell.logoutOpen }
    }
}
