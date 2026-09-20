import QtQuick
import QtQuick.Effects
import Quickshell
import Quickshell.Wayland
import "../style"

PanelWindow {
    id: panel
    required property var shell
    required property var interaction
    readonly property bool opened: Boolean(interaction.active && interaction.ready) && !shell.stopping
    property real reveal: opened ? 1 : 0
    visible: opened || reveal > 0
    anchors { top: true; bottom: true; left: true; right: true }
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Overlay
    WlrLayershell.keyboardFocus: WlrKeyboardFocus.None
    WlrLayershell.namespace: "lunadash-window-switcher"
    color: "transparent"
    Behavior on reveal { NumberAnimation { duration: Theme.motion; easing.type: Easing.OutCubic } }
    Item {
        anchors.fill: parent
        opacity: panel.reveal
        Image {
            id: background
            anchors.fill: parent
            source: panel.interaction.background || panel.shell.state.wallpaperImage || ""
            fillMode: Image.PreserveAspectCrop
            visible: false
            cache: false
        }
        MultiEffect {
            anchors.fill: background
            source: background
            blurEnabled: true
            blurMax: 48
            blur: panel.reveal
            saturation: -0.25
        }
        Rectangle { anchors.fill: parent; color: Theme.background; opacity: 0.60 }
        MouseArea { anchors.fill: parent; onClicked: panel.shell.command("switch-cancel", "") }
        SwitcherContent {
            anchors.centerIn: parent
            width: Math.min(parent.width - 40, 1240)
            height: Math.min(parent.height - 40, 480)
            shell: panel.shell
            selection: panel.interaction
            scale: 0.94 + 0.06 * panel.reveal
            transform: Translate { y: 18 * (1 - panel.reveal) }
        }
    }
}
