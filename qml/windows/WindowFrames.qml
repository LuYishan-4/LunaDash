import QtQuick
import Quickshell
import Quickshell.Wayland
import "../style"

PanelWindow {
    id: panel
    required property var shell
    required property var interaction
    visible: !shell.stopping
    anchors { top: true; bottom: true; left: true; right: true }
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Top
    WlrLayershell.keyboardFocus: WlrKeyboardFocus.None
    WlrLayershell.namespace: "lunadash-window-frames"
    mask: Region {}
    color: "transparent"
    readonly property var windows: (interaction.clients || []).filter(client => !client.desktop && !client.floating && !client.minimized && !client.hiddenByMaximize && Number(client.workspace) === Number(interaction.workspace))
    Repeater {
        model: panel.windows.length
        delegate: Rectangle {
            required property int index
            readonly property var client: panel.windows[index] || ({})
            x: Number(client.x || 0); y: Number(client.y || 0)
            width: Number(client.width || 0); height: Number(client.height || 0)
            radius: 12
            color: "transparent"
            border.color: client.focused ? Theme.accent : Theme.border
            border.width: client.focused ? 2 : 1
            Behavior on border.color { ColorAnimation { duration: Theme.motionFast } }
            Behavior on x { enabled: !panel.interaction.dragging; NumberAnimation { duration: Theme.motion; easing.type: Easing.OutCubic } }
            Behavior on y { enabled: !panel.interaction.dragging; NumberAnimation { duration: Theme.motion; easing.type: Easing.OutCubic } }
            Behavior on width { enabled: !panel.interaction.dragging; NumberAnimation { duration: Theme.motion; easing.type: Easing.OutCubic } }
            Behavior on height { enabled: !panel.interaction.dragging; NumberAnimation { duration: Theme.motion; easing.type: Easing.OutCubic } }
        }
    }
}
