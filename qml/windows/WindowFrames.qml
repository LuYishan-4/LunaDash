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
    readonly property var windows: (interaction.clients || []).filter(client => (shell.state.layoutMode !== "stacking" || client.focused) && !client.desktop && !client.utility && !client.fullscreen && !client.minimized && !client.hiddenByMaximize && client.visible !== false && Number(client.workspace) === Number(interaction.workspace))
    Repeater {
        model: panel.windows.length
        delegate: Rectangle {
            required property int index
            readonly property var client: panel.windows[index] || ({})
            x: Number(client.frameX ?? client.x ?? 0)
            y: Number(client.frameY ?? client.y ?? 0)
            width: Number(client.frameWidth ?? client.width ?? 0)
            height: Number(client.frameHeight ?? client.height ?? 0)
            radius: Math.max(0, Math.min(Number(client.cornerRadius ?? 16), width / 2, height / 2))
            color: "transparent"
            border.color: client.focused
                ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.94)
                : Qt.rgba(Theme.border.r, Theme.border.g, Theme.border.b, 0.72)
            border.width: client.focused ? 2 : 1

            Behavior on x {
                enabled: Theme.animations
                NumberAnimation { duration: 48; easing.type: Easing.Linear }
            }
            Behavior on y {
                enabled: Theme.animations
                NumberAnimation { duration: 48; easing.type: Easing.Linear }
            }
            Behavior on width {
                enabled: Theme.animations
                NumberAnimation { duration: 48; easing.type: Easing.Linear }
            }
            Behavior on height {
                enabled: Theme.animations
                NumberAnimation { duration: 48; easing.type: Easing.Linear }
            }

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
        }
    }
}
