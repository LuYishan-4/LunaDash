import QtQuick
import Quickshell
import "../style"
PanelWindow {
    id: panel
    property bool opened: false
    property real reveal: opened ? 1 : 0
    visible: opened || reveal > 0
    contentItem.opacity: reveal
    contentItem.scale: 0.96 + 0.04 * reveal
    contentItem.transformOrigin: Item.Center
    Behavior on reveal { NumberAnimation { duration: Theme.motion; easing.type: Easing.OutCubic } }
}
