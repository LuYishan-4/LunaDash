
import QtQuick
import "../style"

Rectangle {
    id: cell
    required property var shell
    required property var group
    readonly property var visibleMembers: (group.members || []).slice(0, 4)

    radius: Math.min(13, height / 2)
    color: group.focused
        ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.26)
        : Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.08)
    border.width: group.focused ? 2 : 1
    border.color: group.focused
        ? Theme.accent
        : Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.30)

    Row {
        anchors.centerIn: parent
        spacing: 4
        Repeater {
            model: cell.visibleMembers
            delegate: MemberIcon {
                required property var modelData
                shell: cell.shell
                member: modelData
                grouped: (cell.group.members || []).length > 1
            }
        }
    }

    Behavior on color { ColorAnimation { duration: Theme.motion } }
    Behavior on border.color { ColorAnimation { duration: Theme.motion } }
    Behavior on width { NumberAnimation { duration: Theme.motion; easing.type: Easing.OutCubic } }
}
