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
    scale: cellHover.hovered ? 1.018 : 1
    color: Qt.rgba(Theme.surfaceOpaque.r, Theme.surfaceOpaque.g, Theme.surfaceOpaque.b,
                   group.active ? 0.88 : cellHover.hovered ? 0.72 : 0.52)
    border.width: 1
    border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b,
                          group.active ? 0.64 : cellHover.hovered ? 0.34 : 0.16)
    Accessible.role: Accessible.Grouping
    Accessible.name: shell.tr("Workspace") + " " + (Number(group.workspace || 0) + 1)

    HoverHandler { id: cellHover }

    Rectangle {
        visible: cell.group.active
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 2
        width: Math.min(30, Math.max(12, parent.width * 0.32))
        height: 2
        radius: 1
        color: Theme.accent
    }

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
    Behavior on scale { NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }
}
