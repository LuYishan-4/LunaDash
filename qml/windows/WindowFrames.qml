import QtQuick
import "../plugins"
import Quickshell
import Quickshell.Wayland
import "../style"

PluginPanel {
    id: panel
    extensionTarget: "window-decoration"
    extensionContext: ({interaction: interaction})
    required property var interaction
    visible: !shell.stopping
    anchors { top: true; bottom: true; left: true; right: true }
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Top
    WlrLayershell.keyboardFocus: WlrKeyboardFocus.None
    WlrLayershell.namespace: "lunadash-window-frames"
    mask: Region {}
    color: "transparent"
    readonly property var windows: (interaction.clients || []).filter(client => (shell.state.layoutMode !== "stacking" || client.focused) && !client.desktop && !client.floating && !client.minimized && !client.hiddenByMaximize && Number(client.workspace) === Number(interaction.workspace))
    Repeater {
        model: panel.windows.length
        delegate: Rectangle {
            required property int index
            readonly property var client: panel.windows[index] || ({})
            x: Number(client.x || 0); y: Number(client.y || 0)
            width: Number(client.width || 0); height: Number(client.height || 0)
            radius: 16
            color: "transparent"
            border.color: client.focused
                ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.94)
                : Qt.rgba(Theme.border.r, Theme.border.g, Theme.border.b, 0.72)
            border.width: client.focused ? 2 : 1

            Rectangle {
                visible: parent.client.focused
                anchors.fill: parent
                anchors.margins: -4
                radius: parent.radius + 4
                color: "transparent"
                border.width: 1
                border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.18)
                opacity: 0.9
            }
            Behavior on border.color { ColorAnimation { duration: Theme.motionFast } }
            Behavior on x { enabled: !panel.interaction.dragging; NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }
            Behavior on y { enabled: !panel.interaction.dragging; NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }
            Behavior on width { enabled: !panel.interaction.dragging; NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }
            Behavior on height { enabled: !panel.interaction.dragging; NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }
        }
    }
}
