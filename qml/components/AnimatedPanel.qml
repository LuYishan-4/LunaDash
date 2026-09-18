import QtQuick
import Quickshell
import "../style"

PanelWindow {
    id: panel
    property bool opened: false
    property real reveal: opened ? 1 : 0
    property real entranceOffset: 14

    visible: opened || reveal > 0
    contentItem.opacity: reveal
    contentItem.scale: 0.965 + 0.035 * reveal
    contentItem.transformOrigin: Item.Center
    contentItem.transform: Translate {
        y: (1 - panel.reveal) * panel.entranceOffset
    }

    Behavior on reveal {
        NumberAnimation {
            duration: panel.opened ? Theme.motion : Theme.motionFast
            easing.type: panel.opened ? Easing.OutQuint : Easing.InCubic
        }
    }
}
