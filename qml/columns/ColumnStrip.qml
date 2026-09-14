
import "../modules"
import QtQuick
import Quickshell
import Quickshell.Wayland
import "../style"

ModuleSurface {
    id: strip
    moduleId: "columns"
    readonly property var groups: ((shell.state.tiling || {}).groups || [])
    readonly property int stripMargin: Math.max(8, Math.min(strip.moduleMargin, 24))

    function memberCount(group) {
        return Math.min(4, (group.members || []).length)
    }

    function cellWidth(group) {
        const iconWidth = 30
        const iconSpacing = 4
        const memberWidth = memberCount(group) * iconWidth + Math.max(0, memberCount(group) - 1) * iconSpacing + 14
        return Math.max(memberWidth, Math.min(168, Math.max(52, Number(group.width || 0) * 0.18)))
    }

    function desiredWidth() {
        let width = 12
        for (let index = 0; index < groups.length; ++index)
            width += cellWidth(groups[index]) + (index > 0 ? 5 : 0)
        return width
    }

    anchors { top: true; left: true }
    margins { top: Theme.barHeight + Math.max(6, Math.min(strip.moduleMargin, 12)); left: strip.stripMargin }
    implicitWidth: moduleWidth(screen ? Math.min(desiredWidth(), screen.width - 2 * strip.stripMargin) : desiredWidth())
    implicitHeight: moduleHeight(48)
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Top
    WlrLayershell.namespace: "lunadah-columns"
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
