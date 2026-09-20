
import "../modules"
import QtQuick
import Quickshell
import Quickshell.Wayland
import "../style"

ModuleSurface {
    id: strip
    moduleId: "columns"
    readonly property var groups: ((shell.interaction || {}).clients || shell.state.clients || [])
        .filter(client => !client.desktop && client.mapped)
        .map(client => ({focused: client.focused, members: [{window: client.id, title: client.title,
            appId: client.appId, icon: client.icon, workspace: client.workspace, minimized: client.minimized, focused: client.focused}]}))
    readonly property int stripMargin: Math.max(8, Math.min(strip.moduleMargin, 24))
    function cellWidth(group) { return 40 }
    function desiredWidth() { return 12 + groups.length * 45 }

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
        spacing: 5
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        model: strip.groups
        delegate: ColumnCell {
            required property var modelData
            required property int index
            shell: strip.shell
            group: modelData
            width: strip.cellWidth(modelData)
            height: columns.height
        }
    }
}
