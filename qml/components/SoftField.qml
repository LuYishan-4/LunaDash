import QtQuick
import QtQuick.Controls
import "../style"

TextField {
    id: control
    property bool invalid: false

    implicitHeight: 40
    leftPadding: 12
    rightPadding: 12
    color: Theme.text
    placeholderTextColor: Theme.muted
    font.family: Theme.font
    font.pixelSize: 12
    selectByMouse: true
    persistentSelection: true
    focusPolicy: Qt.StrongFocus
    activeFocusOnPress: true

    TapHandler {
        acceptedButtons: Qt.LeftButton
        onTapped: control.forceActiveFocus(Qt.MouseFocusReason)
    }

    background: Rectangle {
        radius: 12
        color: control.activeFocus ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.08) : Theme.surface
        border.width: control.activeFocus || control.invalid ? 2 : 1
        border.color: control.invalid ? Theme.danger : control.activeFocus ? Theme.accent : Theme.border
        Behavior on color { ColorAnimation { duration: Theme.motion } }
        Behavior on border.color { ColorAnimation { duration: Theme.motion } }
    }
}
