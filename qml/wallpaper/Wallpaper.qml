import QtQuick
import Quickshell
import Quickshell.Wayland
PanelWindow {
    required property var shell
    anchors { top: true; bottom: true; left: true; right: true }
    WlrLayershell.layer: WlrLayer.Background
    WlrLayershell.namespace: "ludash-wallpaper"
    exclusionMode: ExclusionMode.Ignore
    color: "transparent"
    Text {
        x: 40; y: 100
        text: "L U D A S H  /  " + (shell.state.graphicsApi || "OPENGL")
        color: "#c3cde1"; font.pixelSize: 12; font.letterSpacing: 2
    }
}
