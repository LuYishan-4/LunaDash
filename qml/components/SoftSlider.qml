import QtQuick
import QtQuick.Controls
import "../style"
Slider {
    id: control
    implicitHeight: 30
    background: Rectangle {
        x: control.leftPadding; y: control.topPadding + control.availableHeight / 2 - height / 2
        width: control.availableWidth; height: 4; radius: 2; color: "#394656"
        Rectangle { width: control.visualPosition * parent.width; height: parent.height; radius: 2; color: Theme.accent }
    }
    handle: Rectangle {
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: control.topPadding + control.availableHeight / 2 - height / 2
        width: control.pressed ? 20 : 16; height: width; radius: width / 2
        color: Theme.accent; border.color: "#dce8f6"; border.width: control.activeFocus ? 2 : 0
        Behavior on width { NumberAnimation { duration: Math.min(Theme.motion, 120) } }
    }
}
