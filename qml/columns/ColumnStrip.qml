
import "../modules"
import QtQuick
import "WorkspaceTasks.js" as WorkspaceTasks
import Quickshell
import Quickshell.Wayland
import "../style"

ModuleSurface {
    id: strip
    moduleId: "columns"
    readonly property var groups: WorkspaceTasks.groupByWorkspace(
        (shell.interaction || {}).clients || shell.state.clients || [],
        (shell.interaction || {}).workspace ?? shell.state.workspace)
    readonly property int stripMargin: Math.max(8, Math.min(strip.moduleMargin, 24))
    function cellWidth(group) { return 12 + group.members.length * Math.max(16, columns.height - 8) + Math.max(0, group.members.length - 1) * 4 }
    function desiredWidth() { return 12 + groups.reduce((width, group) => width + cellWidth(group) + 8, 0) }

    anchors { top: true; left: true }
    margins { top: Theme.barHeight + Math.max(6, Math.min(strip.moduleMargin, 12)); left: strip.stripMargin }
    implicitWidth: moduleWidth(screen ? Math.min(desiredWidth(), screen.width - 2 * strip.stripMargin) : desiredWidth())
    implicitHeight: moduleHeight(48)
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Top
    WlrLayershell.namespace: "lunadash-columns"
    color: "transparent"

    Rectangle {
        anchors.fill: parent
        radius: Math.min(strip.moduleRadius, height / 2)
        color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.12)
        border.width: 1
        border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.46)
        Behavior on color { ColorAnimation { duration: Theme.motion } }
        Behavior on border.color { ColorAnimation { duration: Theme.motion } }
    }

    ListView {
        id: columns
        anchors.fill: parent
        anchors.margins: 6
        orientation: ListView.Horizontal
        spacing: 8
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        model: strip.groups.length
        delegate: ColumnCell {
            required property int index
            shell: strip.shell
            group: strip.groups[index] || ({members: []})
            width: implicitWidth
            height: columns.height
        }
    }
}
