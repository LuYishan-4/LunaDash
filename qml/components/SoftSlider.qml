import QtQuick
import QtQuick.Controls
import "../style"
Slider {
    id: control
    implicitHeight: 30
    background: Rectangle {
        x: control.leftPadding; y: control.topPadding + control.availableHeight / 2 - height / 2
        width: control.availableWidth; height: 4; radius: 2; color: Theme.track
        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            radius: 2
            color: Theme.starlight
            Behavior on width { NumberAnimation { duration: Theme.motion; easing.type: Easing.OutCubic } }
        }
    }
    handle: Rectangle {
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: control.topPadding + control.availableHeight / 2 - height / 2
        width: control.pressed ? 20 : 16; height: width; radius: width / 2
        color: Theme.moon
        border.color: control.activeFocus ? Theme.accent : Theme.focusRing
        border.width: control.activeFocus ? 2 : 1
        Rectangle { width: 4; height: 4; radius: 2; anchors.centerIn: parent; color: Theme.secondaryAccent }
        Behavior on width { NumberAnimation { duration: Math.min(Theme.motion, 120) } }
        Behavior on border.color { ColorAnimation { duration: Theme.motion } }
    }
}
