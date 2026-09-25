import QtQuick
import "../plugins"
import Quickshell
import Quickshell.Wayland
import "../style"

PluginPanel {
    id: panel
    extensionTarget: "tiling-hint"
    required property var drag
    visible: Boolean(drag.target) && !shell.stopping
    anchors { top: true; bottom: true; left: true; right: true }
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Overlay
    WlrLayershell.keyboardFocus: WlrKeyboardFocus.None
    WlrLayershell.namespace: "lunadash-tiling-drop"
    mask: Region {}
    color: "transparent"
    Rectangle {
        x: Number(panel.drag.x || 0)
        y: Number(panel.drag.y || 0) + (Number(panel.drag.edge) > 0 ? Number(panel.drag.height || 0) - 6 : 0)
        width: Number(panel.drag.width || 0)
        height: panel.drag.edge ? 6 : Number(panel.drag.height || 0)
        radius: panel.drag.edge ? 4 : 18
        color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b,
                       panel.drag.edge ? 0.88 : 0.14)
        border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.92)
        border.width: 2
        scale: visible ? 1 : 0.97
        Behavior on x { NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }
        Behavior on y { NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }
        Behavior on width { NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }
        Behavior on height { NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }
        Behavior on scale { NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }
    }
}
