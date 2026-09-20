import QtQuick
import QtQuick.Controls
import "../components"
import "../style"

Rectangle {
    id: selector
    required property var shell
    required property var selection
    readonly property var workspaces: selection.workspaces || []
    radius: 24
    color: Qt.rgba(Theme.background.r, Theme.background.g, Theme.background.b, 0.96)
    border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.45)
    border.width: 1
    clip: true
    Text {
        x: 24; y: 18; width: parent.width - 48
        text: selector.shell.tr("Workspaces")
        color: Theme.text; font.family: Theme.font; font.pixelSize: 18; font.bold: true
        elide: Text.ElideRight
    }
    Grid {
        id: grid
        anchors { left: parent.left; right: parent.right; top: parent.top; bottom: hint.top; margins: 20; topMargin: 54; bottomMargin: 12 }
        columns: 5
        rows: 2
        spacing: 10
        Repeater {
            model: 10
            delegate: Rectangle {
                id: cell
                required property int index
                readonly property var workspace: selector.workspaces[index] || ({id: index + 1, windows: []})
                readonly property bool selected: index === Number(selector.selection.index || 0)
                width: (grid.width - 4 * grid.spacing) / 5
                height: (grid.height - grid.spacing) / 2
                radius: 12
                color: selected ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.19) : Theme.surface
                border.color: selected ? Theme.accent : Theme.border
                border.width: selected ? 2 : 1
                scale: selected ? 1 : 0.96
                Behavior on color { ColorAnimation { duration: Theme.motionFast } }
                Behavior on scale { NumberAnimation { duration: Theme.motion; easing.type: Easing.OutCubic } }
                Text {
                    anchors.centerIn: parent
                    text: cell.index + 1
                    color: Theme.muted
                    opacity: 0.42
                    font.family: Theme.font
                    font.pixelSize: Math.min(36, cell.height * 0.24)
                    font.bold: true
                }
                Item {
                    id: preview
                    anchors { fill: parent; margins: 6; bottomMargin: 25 }
                    clip: true
                    readonly property real ratio: Math.min(width / Math.max(1, Number(cell.workspace.width || 1440)), height / Math.max(1, Number(cell.workspace.height || 900)))
                    readonly property real offsetX: (width - Number(cell.workspace.width || 1440) * ratio) / 2
                    readonly property real offsetY: (height - Number(cell.workspace.height || 900) * ratio) / 2
                    Repeater {
                        model: (cell.workspace.windows || []).length
                        delegate: Rectangle {
                            required property int index
                            readonly property var member: (cell.workspace.windows || [])[index] || ({})
                            x: preview.offsetX + Number(member.x || 0) * preview.ratio
                            y: preview.offsetY + Number(member.y || 0) * preview.ratio
                            width: Math.max(1, Number(member.width || 720) * preview.ratio)
                            height: Math.max(1, Number(member.height || 500) * preview.ratio)
                            visible: !member.minimized
                            color: Theme.surfaceOpaque
                            border.color: member.focused ? Theme.accent : Theme.border
                            border.width: 1
                            clip: true
                            Image {
                                anchors { fill: parent; margins: 1 }
                                source: parent.member.thumbnail || ""
                                fillMode: Image.Stretch
                                asynchronous: true
                                cache: false
                            }
                            ApplicationIcon {
                                visible: !parent.member.thumbnail
                                shell: selector.shell
                                anchors.centerIn: parent
                                width: Math.max(1, Math.min(28, parent.width - 4, parent.height - 4)); height: width
                                iconName: String(parent.member.icon || "")
                                appId: String(parent.member.appId || "")
                                title: String(parent.member.title || "")
                            }
                        }
                    }
                }
                Text {
                    anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 8 }
                    text: String(cell.index + 1) + "  ·  " + (cell.workspace.windows || []).length
                    color: cell.selected ? Theme.text : Theme.muted
                    font.family: Theme.font; font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        selector.shell.command("switch-window", cell.index + 1)
                        selector.shell.command("switch-accept", "")
                    }
                }
            }
        }
    }
    Text {
        id: hint
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 18 }
        text: selector.shell.tr("Release Alt to switch workspace · Esc to cancel")
        color: Theme.muted; font.family: Theme.font; font.pixelSize: 12
        horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap
    }
}
