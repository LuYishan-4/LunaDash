import QtQuick
import QtQuick.Shapes
import Quickshell
import Quickshell.Wayland
PanelWindow {
    required property var shell
    anchors { top: true; bottom: true; left: true; right: true }
    WlrLayershell.layer: WlrLayer.Background
    WlrLayershell.namespace: "ludash-wallpaper"
    exclusionMode: ExclusionMode.Ignore
    color: "#18243b"
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0; color: shell.state.wallpaper === 1 ? "#15414a" : "#262b48" }
            GradientStop { position: 1; color: shell.state.wallpaper === 1 ? "#577e78" : "#b18f9c" }
        }
    }
    Rectangle {
        x: parent.width * 0.72; y: parent.height * 0.17
        width: parent.width * 0.17; height: width; radius: width / 2
        color: shell.state.wallpaper === 1 ? "#bfd8b8" : "#f0c4b2"
    }
    Repeater {
        model: 6
        Shape {
            id: mountain
            required property int index
            width: parent.width; height: parent.height
            ShapePath {
                strokeWidth: 0
                fillColor: ["#817b94", "#5d6582", "#424f71", "#2c405f", "#20334f", "#172940"][mountain.index]
                startX: 0; startY: mountain.height * (0.48 + mountain.index * 0.075)
                PathCubic { x: mountain.width; y: mountain.height * (0.52 + mountain.index * 0.08); control1X: mountain.width * 0.3; control1Y: mountain.height * (0.1 + mountain.index * 0.09); control2X: mountain.width * 0.7; control2Y: mountain.height * (0.85 + mountain.index * 0.02) }
                PathLine { x: mountain.width; y: mountain.height }
                PathLine { x: 0; y: mountain.height }
            }
        }
    }
}
