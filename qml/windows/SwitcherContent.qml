import QtQuick
import "../components"
import "../style"

Rectangle {
    id: selector
    required property var shell
    required property var selection
    readonly property var workspaces: selection.workspaces || []
    radius: 8
    color: Qt.rgba(Theme.background.r, Theme.background.g, Theme.background.b, 0.92)
    border.color: Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.32)
    border.width: 1
    clip: true
    Grid {
        id: grid
        anchors { fill: parent; margins: 5 }
        columns: 5
        rows: 2
        spacing: 2
        Repeater {
            model: 10
            delegate: Item {
                id: cell
                required property int index
                readonly property var workspace: selector.workspaces[index] || ({id: index + 1, windows: []})
                readonly property bool selected: index === Number(selector.selection.index || 0)
                width: (grid.width - 4 * grid.spacing) / 5
                height: (grid.height - grid.spacing) / 2
                Accessible.role: Accessible.Button
                Accessible.name: selector.shell.tr("Workspace") + " " + (index + 1)
                Accessible.focused: selected
                Text {
                    anchors.centerIn: parent
                    visible: !(cell.workspace.windows || []).some(window => !window.minimized && !window.hiddenByMaximize)
                    text: cell.index + 1
                    color: Theme.text
                    opacity: 0.25
                    font.family: Theme.font
                    font.pixelSize: Math.min(28, cell.height * 0.20)
                    font.bold: true
                }
                Item {
                    id: preview
                    anchors { fill: parent; margins: 7 }
                    clip: true
                    readonly property real ratio: Math.min(width / Math.max(1, Number(cell.workspace.width || 1440)), height / Math.max(1, Number(cell.workspace.height || 900)))
                    readonly property real offsetX: (width - Number(cell.workspace.width || 1440) * ratio) / 2
                    readonly property real offsetY: (height - Number(cell.workspace.height || 900) * ratio) / 2
                    Repeater {
                        model: (cell.workspace.windows || []).length
                        delegate: Item {
                            id: windowPreview
                            required property int index
                            readonly property var member: (cell.workspace.windows || [])[index] || ({})
                            x: Math.round(preview.offsetX + Number(member.x || 0) * preview.ratio) + 1
                            y: Math.round(preview.offsetY + Number(member.y || 0) * preview.ratio) + 1
                            width: Math.max(1, Math.round(preview.offsetX + (Number(member.x || 0) + Number(member.width || 720)) * preview.ratio) - x - 1)
                            height: Math.max(1, Math.round(preview.offsetY + (Number(member.y || 0) + Number(member.height || 500)) * preview.ratio) - y - 1)
                            visible: !member.minimized && !member.hiddenByMaximize
                            clip: true
                            Rectangle {
                                anchors.fill: parent
                                color: Qt.rgba(Theme.background.r, Theme.background.g, Theme.background.b, 0.65)
                            }
                            Image {
                                anchors { fill: parent; margins: 1 }
                                source: parent.member.thumbnail || ""
                                fillMode: Image.PreserveAspectFit
                                asynchronous: true
                                cache: false
                            }
                            ApplicationIcon {
                                shell: selector.shell
                                anchors.centerIn: parent
                                width: Math.max(1, Math.min(44, parent.width - 6, parent.height - 6)); height: width
                                iconName: String(parent.member.icon || "")
                                appId: String(parent.member.appId || "")
                                title: String(parent.member.title || "")
                            }
                            Rectangle {
                                anchors.fill: parent
                                color: "transparent"
                                border.color: Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, windowPreview.member.focused ? 0.85 : 0.55)
                                border.width: 1
                            }
                        }
                    }
                }
                MouseArea {
                    id: cellMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onEntered: selector.shell.command("switch-window", cell.index + 1)
                    onClicked: selector.shell.command("switch-accept", "")
                }
            }
        }
    }
    Rectangle {
        x: grid.x + (Number(selector.selection.index || 0) % 5) * ((grid.width - 4 * grid.spacing) / 5 + grid.spacing)
        y: grid.y + Math.floor(Number(selector.selection.index || 0) / 5) * ((grid.height - grid.spacing) / 2 + grid.spacing)
        width: (grid.width - 4 * grid.spacing) / 5
        height: (grid.height - grid.spacing) / 2
        radius: 3
        color: "transparent"
        border.color: Theme.text
        border.width: 1
        Behavior on x { NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }
        Behavior on y { NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }
    }
}
