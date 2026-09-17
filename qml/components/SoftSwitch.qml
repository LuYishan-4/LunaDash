import QtQuick
import QtQuick.Controls
import "../style"
Switch {
    id: control
    spacing: 10; implicitHeight: 38
    indicator: Rectangle {
        implicitWidth: 40; implicitHeight: 22; x: control.leftPadding; y: control.height / 2 - height / 2
        radius: 11
        color: control.checked ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.78) : Theme.track
        border.width: control.activeFocus ? 1 : 0
        border.color: Theme.moon
        Rectangle {
            x: control.checked ? parent.width - width - 3 : 3
            y: 3
            width: 16; height: 16; radius: 8
            color: control.checked ? Theme.moon : Theme.knob
            border.width: 1
            border.color: Qt.rgba(Theme.starlight.r, Theme.starlight.g, Theme.starlight.b, 0.55)
            Rectangle {
                visible: control.checked
                width: 5; height: 5; radius: 2.5
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.rightMargin: 2
                anchors.topMargin: 2
                color: Theme.secondaryAccent
            }
            Behavior on x { NumberAnimation { duration: Math.min(Theme.motion, 160); easing.type: Easing.OutCubic } }
        }
        Behavior on color { ColorAnimation { duration: Theme.motion } }
    }
    contentItem: Text { text: control.text; color: Theme.text; font.family: Theme.font; font.pixelSize: 13; verticalAlignment: Text.AlignVCenter; leftPadding: control.indicator.width + control.spacing }
}
