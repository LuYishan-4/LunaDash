import QtQuick
import "../style"

Rectangle {
    id: cell
    required property var shell
    required property var group
    readonly property var visibleMembers: group.members || []
    readonly property int memberSize: Math.max(16, height - 8)
    implicitWidth: 12 + visibleMembers.length * memberSize + Math.max(0, visibleMembers.length - 1) * 4
    implicitHeight: 32
    radius: height / 2
    color: Qt.tint(Theme.surfaceOpaque,
        Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, group.active ? 0.36 : 0.06))
    border.width: 1
    border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, group.active ? 0.90 : 0.24)
    Accessible.role: Accessible.Grouping
    Accessible.name: shell.tr("Workspace") + " " + (Number(group.workspace || 0) + 1)

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        cursorShape: Qt.PointingHandCursor
        property int pressedWorkspace: -1
        onPressed: pressedWorkspace = Number(cell.group.workspace)
        onClicked: {
            if (pressedWorkspace >= 0 && pressedWorkspace === Number(cell.group.workspace))
                cell.shell.command("workspace", pressedWorkspace)
        }
        onCanceled: pressedWorkspace = -1
    }

    Row {
        anchors.centerIn: parent
        spacing: 4
        opacity: cell.group.active ? 1 : 0.68
        Behavior on opacity { NumberAnimation { duration: Theme.motionFast } }
        Repeater {
            // Keep delegates stable while a task holds a pointer press.
            model: cell.visibleMembers.length
            delegate: MemberIcon {
                required property int index
                shell: cell.shell
                member: cell.visibleMembers[index] || ({})
                width: cell.memberSize
                height: cell.memberSize
            }
        }
    }

    Behavior on color { ColorAnimation { duration: Theme.motion } }
    Behavior on border.color { ColorAnimation { duration: Theme.motion } }
    Behavior on width { NumberAnimation { duration: Theme.motion; easing.type: Easing.OutCubic } }
}
