import QtQuick
import QtQuick.Controls
import "../style"
Switch {
    id: control
    spacing: 10; implicitHeight: 38
    indicator: Rectangle {
        implicitWidth: 38; implicitHeight: 22; x: control.leftPadding; y: control.height / 2 - height / 2
        radius: 11; color: control.checked ? Theme.accent : "#3a4859"
        border.width: control.activeFocus ? 1 : 0; border.color: Theme.text
        Rectangle { x: control.checked ? parent.width - width - 3 : 3; y: 3; width: 16; height: 16; radius: 8; color: control.checked ? "#17212e" : "#b6c3d3"; Behavior on x { NumberAnimation { duration: Math.min(Theme.motion, 160) } } }
    }
    contentItem: Text { text: control.text; color: Theme.text; font.family: Theme.font; font.pixelSize: 13; verticalAlignment: Text.AlignVCenter; leftPadding: control.indicator.width + control.spacing }
}
