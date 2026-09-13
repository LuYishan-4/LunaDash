import "../modules"
import QtQuick
import Quickshell
import Quickshell.Wayland
import "../components"
import "../style"
ModuleSurface {
    id: panel
    moduleId: "panel"
    anchors { top: moduleStyle.edge !== "bottom"; bottom: moduleStyle.edge === "bottom"; left: !moduleStyle.width; right: !moduleStyle.width }
    margins { top: moduleMargin; bottom: moduleMargin; left: moduleMargin; right: moduleMargin }
    implicitWidth: moduleWidth(1440)
    implicitHeight: moduleHeight((shell.state.appearance || {}).panelHeight || 40)
    exclusiveZone: implicitHeight + moduleMargin * 2
    color: "transparent"
    WlrLayershell.namespace: "ludash-panel"
    property var stats: shell.state.system || ({})
    Rectangle { anchors.fill: parent; color: moduleBackground; radius: moduleRadius }
    Row {
        anchors { left: parent.left; leftMargin: 10; verticalCenter: parent.verticalCenter } spacing: 5
        PanelSegment { moduleHost: panel; text: "✦"; fill: moduleAccent; ink: "#102133"; onClicked: shell.launcherOpen = !shell.launcherOpen }
        Repeater {
            model: (shell.state.appearance || {}).workspaceCount || 4
            PanelSegment { moduleHost: panel; required property int index; text: String(index + 1); implicitWidth: moduleWidth(panel.width < 1100 ? 24 : shell.state.workspace === index ? 44 : 30); selected: shell.state.workspace === index; onClicked: shell.command("workspace", index) }
        }
        PanelSegment { moduleHost: panel; text: "⌘"; onClicked: shell.launch("terminal") }
    }
    Row {
        anchors.centerIn: parent; spacing: 5
        PanelSegment { moduleHost: panel; text: "◈"; ink: moduleAccent; onClicked: shell.setAppearance({ overview: !shell.overviewOpen }) }
        PanelSegment { moduleHost: panel; text: shell.focusedTitle.slice(0, panel.width > 1100 ? 32 : 16); fill: Theme.surface; onClicked: shell.launcherOpen = !shell.launcherOpen }
    }
    Row {
        anchors { right: parent.right; rightMargin: 10; verticalCenter: parent.verticalCenter } spacing: 4
        PanelSegment { moduleHost: panel; visible: panel.width > 1250; text: "CPU  " + (panel.stats.cpuPercent || 0) + "%"; ink: Theme.muted; onClicked: shell.launch("monitor") }
        PanelSegment { moduleHost: panel;
            id: clock; property string time: ""; text: time
            onClicked: shell.setAppearance({ overview: !shell.overviewOpen })
            Timer { interval: 1000; repeat: true; running: true; triggeredOnStart: true; onTriggered: clock.time = Qt.formatDateTime(new Date(), Theme.clock24Hour ? "ddd  HH:mm" : "ddd  h:mm AP") }
        }
        PanelSegment { moduleHost: panel; text: (shell.state.network || {}).connected ? "↔" : "×"; ink: (shell.state.network || {}).internet ? Theme.accent : Theme.muted; onClicked: shell.settingsOpen = !shell.settingsOpen }
        PanelSegment { moduleHost: panel; visible: (panel.stats.batteryPercent ?? -1) >= 0; text: panel.stats.batteryPercent + "%"; onClicked: shell.launch("monitor") }
        PanelSegment { moduleHost: panel; text: "⚙"; fill: Theme.surface; onClicked: shell.settingsOpen = !shell.settingsOpen }
        PanelSegment { moduleHost: panel; text: "⏻"; ink: Theme.danger; onClicked: shell.logoutOpen = !shell.logoutOpen }
    }
}
