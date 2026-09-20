
import QtQuick
import "../style"

Rectangle {
    id: cell
    required property var shell
    required property var group
    readonly property var visibleMembers: (group.members || []).slice(0, 8)

    radius: Math.min(13, height / 2)
    color: group.focused
        ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.26)
        : Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.08)
    border.width: group.focused ? 2 : 1
    border.color: group.focused
        ? Theme.accent
        : Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.30)

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        property var pressedWindow: 0
        onPressed: {
            const members = cell.group.members || []
            const target = members.find(member => member.focused)
                || members.find(member => !member.minimized) || members[0]
            pressedWindow = target ? target.window : 0
        }
        onClicked: {
            if (pressedWindow && (cell.group.members || []).some(member => member.window === pressedWindow))
                cell.shell.command("activate-window", pressedWindow)
        }
    }

    Row {
        anchors.centerIn: parent
        spacing: 4
        Repeater {
            // Polling replaces the JSON array, but must not replace a delegate
            // while it holds a mouse grab. MemberIcon captures the pressed ID.
            model: cell.visibleMembers.length
            delegate: MemberIcon {
                required property int index
                shell: cell.shell
                member: cell.visibleMembers[index] || ({})
            }
        }
    }

    Behavior on color { ColorAnimation { duration: Theme.motion } }
    Behavior on border.color { ColorAnimation { duration: Theme.motion } }
    Behavior on width { NumberAnimation { duration: Theme.motion; easing.type: Easing.OutCubic } }
}
