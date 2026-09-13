import QtQuick
import Quickshell
import Quickshell.Wayland
import "../style"
PanelWindow {
    required property var shell
    anchors.top: true
    margins.top: 42
    implicitWidth: 460; implicitHeight: 74
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Overlay
    WlrLayershell.namespace: "ludash-message"
    color: "transparent"
    Rectangle { anchors.fill: parent; radius: 6; color: Theme.background; border.color: Theme.danger }
    Text { anchors.fill: parent; anchors.margins: 16; text: shell.tr(shell.errorMessage); wrapMode: Text.WordWrap; color: Theme.text; font.family: Theme.font; font.pixelSize: 12 }
    MouseArea { anchors.fill: parent; onClicked: shell.errorMessage = "" }
}
