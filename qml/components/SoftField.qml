import QtQuick
import QtQuick.Controls
import "../style"

// Shared text input for Theme-based surfaces. Every settings page uses this
// instead of writing its own background, so fields keep one shape: surface
// fill, 12 px radius and an accent focus ring.
TextField {
    id: control
    property bool invalid: false

    implicitHeight: 38
    leftPadding: 12
    rightPadding: 12
    color: Theme.text
    placeholderTextColor: Theme.muted
    font.family: Theme.font
    font.pixelSize: 12
    selectByMouse: true

    background: Rectangle {
        radius: 12
        color: Theme.surface
        border.width: control.activeFocus || control.invalid ? 2 : 1
        border.color: control.invalid ? Theme.danger : control.activeFocus ? Theme.accent : Theme.border
        Behavior on border.color { ColorAnimation { duration: Theme.motion } }
    }
}
