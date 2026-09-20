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
        radius: panel.drag.edge ? 3 : 12
        color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, panel.drag.edge ? 0.9 : 0.18)
        border.color: Theme.accent
        border.width: 2
    }
}
